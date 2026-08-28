#!/usr/bin/env python3
"""Regression tests for untrusted bitmap RDB/RESTORE input (issue #153).

The test mutates valid DUMP payloads, recomputes Redis's CRC64 footer, and sends
them through RESTORE. It covers truncated CRoaring data, structurally invalid
32- and 64-bit bitmaps, and malformed Redis module framing. A client must be
authorized to run RESTORE to reach these loaders.

It talks RESP over a raw socket so it needs no third-party Python packages.
"""

import os
import socket
import struct
import sys

HOST = "127.0.0.1"


# --- Redis CRC64 (jones poly, reflected in/out), ported from deps/redis/src/crc64.c.
# Verified against Redis's own test vectors: crc64("123456789") == 0xe9c6d914c4b8d9ca.
_POLY = 0xAD93D23594C935A9
_MASK = 0xFFFFFFFFFFFFFFFF


def _reflect64(data):
    result = 0
    for i in range(64):
        result = (result << 1) | ((data >> i) & 1)
    return result


def crc64(data):
    crc = 0
    for byte in data:
        i = 1
        while i & 0xFF:
            bit = crc & 0x8000000000000000
            if byte & i:
                bit = 0 if bit else 1
            crc = (crc << 1) & _MASK
            if bit:
                crc ^= _POLY
            i <<= 1
    return _reflect64(crc & _MASK)


class RespClient:
    def __init__(self, host, port):
        self._sock = socket.create_connection((host, port), timeout=30)
        self._buf = b""

    def close(self):
        try:
            self._sock.close()
        except OSError:
            pass

    def command(self, *args):
        parts = [b"*%d\r\n" % len(args)]
        for arg in args:
            if isinstance(arg, str):
                arg = arg.encode()
            elif isinstance(arg, int):
                arg = str(arg).encode()
            parts.append(b"$%d\r\n" % len(arg))
            parts.append(arg)
            parts.append(b"\r\n")
        self._sock.sendall(b"".join(parts))
        return self._read_reply()

    def _read_line(self):
        while b"\r\n" not in self._buf:
            self._fill()
        line, self._buf = self._buf.split(b"\r\n", 1)
        return line

    def _read_n(self, n):
        # n bytes plus the trailing CRLF.
        while len(self._buf) < n + 2:
            self._fill()
        data = self._buf[:n]
        self._buf = self._buf[n + 2:]
        return data

    def _fill(self):
        chunk = self._sock.recv(65536)
        if not chunk:
            raise ConnectionError("server closed the connection")
        self._buf += chunk

    def _read_reply(self):
        line = self._read_line()
        prefix, rest = line[:1], line[1:]
        if prefix == b"+":
            return rest
        if prefix == b"-":
            return RespError(rest.decode(errors="replace"))
        if prefix == b":":
            return int(rest)
        if prefix == b"$":
            length = int(rest)
            if length == -1:
                return None
            return self._read_n(length)
        if prefix == b"*":
            count = int(rest)
            if count == -1:
                return None
            return [self._read_reply() for _ in range(count)]
        raise ValueError("unexpected reply: %r" % line)


class RespError(str):
    """A RESP error reply, kept distinct from a normal string reply."""


# The 32-bit CRoaring ARRAY_UINT32 serialization of the set {0, 1, 2}:
#   byte 0     : format tag CROARING_SERIALIZATION_ARRAY_UINT32 (0x01)
#   bytes 1..4 : cardinality = 3 (little-endian uint32)
#   bytes 5..16: elements 0, 1, 2 (little-endian uint32 each)
# roaring_bitmap_serialize() picks this array form for such a small sparse set,
# and RDB stores the 17-byte buffer verbatim (no compression under 20 bytes), so
# it appears contiguously inside the DUMP payload.
_ARRAY_SIG = struct.pack("<BI", 1, 3) + struct.pack("<III", 0, 1, 2)


def repatch_crc(payload):
    """Return payload with its trailing 8-byte CRC64 footer recomputed."""
    body = payload[:-8]
    return body + struct.pack("<Q", crc64(body))


def read_rdb_length(payload, pos):
    """Decode one Redis RDB length and return (value, next position)."""
    if pos >= len(payload):
        raise AssertionError("truncated RDB length")

    first = payload[pos]
    length_type = first >> 6
    if length_type == 0:
        return first & 0x3F, pos + 1
    if length_type == 1:
        if pos + 2 > len(payload):
            raise AssertionError("truncated 14-bit RDB length")
        return ((first & 0x3F) << 8) | payload[pos + 1], pos + 2
    if first == 0x80:
        if pos + 5 > len(payload):
            raise AssertionError("truncated 32-bit RDB length")
        return struct.unpack(">I", payload[pos + 1:pos + 5])[0], pos + 5
    if first == 0x81:
        if pos + 9 > len(payload):
            raise AssertionError("truncated 64-bit RDB length")
        return struct.unpack(">Q", payload[pos + 1:pos + 9])[0], pos + 9
    raise AssertionError("encoded RDB strings are disabled for this test")


def encode_rdb_length(length):
    if length < 64:
        return bytes([length])
    if length < 16384:
        return bytes([0x40 | (length >> 8), length & 0xFF])
    if length <= 0xFFFFFFFF:
        return b"\x80" + struct.pack(">I", length)
    return b"\x81" + struct.pack(">Q", length)


def module_string_layout(payload):
    """Locate the single RedisModule_SaveStringBuffer value in a DUMP."""
    if len(payload) < 12:
        raise AssertionError("DUMP payload is too short")

    # Skip the object type and module type id, then require the STRING opcode.
    _, pos = read_rdb_length(payload, 1)
    opcode_pos = pos
    opcode, pos = read_rdb_length(payload, pos)
    if opcode != 5:
        raise AssertionError("expected Redis module STRING opcode, got %d" % opcode)

    length_pos = pos
    string_length, data_pos = read_rdb_length(payload, pos)
    data_end = data_pos + string_length
    if data_end >= len(payload) - 10:
        raise AssertionError("module string extends past the DUMP payload")

    end_opcode, footer_pos = read_rdb_length(payload, data_end)
    if end_opcode != 0 or footer_pos != len(payload) - 10:
        raise AssertionError("unexpected data after the module string")
    return opcode_pos, length_pos, data_pos, data_end


def craft_oob_payload(payload, fake_cardinality):
    """Inflate the ARRAY_UINT32 cardinality in a valid DUMP and re-sign it."""
    _, _, data_pos, data_end = module_string_layout(payload)
    if payload[data_pos:data_end] != _ARRAY_SIG:
        raise AssertionError(
            "expected the ARRAY_UINT32 serialization of {0,1,2}; "
            "the 32-bit serialization format may have changed -- update this test"
        )
    tampered = bytearray(payload)
    tampered[data_pos + 1:data_pos + 5] = struct.pack("<I", fake_cardinality)
    return repatch_crc(bytes(tampered))


# A native CONTAINER-tagged (0x02) blob whose portable array container holds two
# UNSORTED uint16 values. roaring_bitmap_deserialize_safe reads it fully in-bounds
# -- the size check alone accepts it -- but the container violates the sorted
# invariant, so roaring_bitmap_internal_validate rejects it. This is the case the
# CRoaring maintainer raised on PR #154: a size-checked deserialize is not enough
# on untrusted input; the structure must also be validated. Generated against the
# pinned CRoaring, where deserialize_safe returns non-NULL and internal_validate
# reports "array elements not strictly increasing". If the vendored CRoaring's
# portable format ever changes, regenerate this blob.
_MALFORMED_PORTABLE_BLOB = (
    b"\x3a\x30\x00\x00\x01\x00\x00\x00\x00\x00\x01\x00\x10\x00\x00\x00\x0a\x00\x05\x00"
)
_MALFORMED_R32_BLOB = b"\x02" + _MALFORMED_PORTABLE_BLOB

# One 64-bit map entry (high bits zero) containing the same malformed portable
# 32-bit bitmap. The safe 64-bit deserializer accepts the byte lengths, while
# roaring64_bitmap_internal_validate must reject the unsorted inner container.
_MALFORMED_R64_BLOB = (
    struct.pack("<Q", 1) + struct.pack("<I", 0) + _MALFORMED_PORTABLE_BLOB
)


def replace_module_string(payload, new_value):
    """Replace a module string in a valid DUMP and re-sign it."""
    _, length_pos, _, data_end = module_string_layout(payload)
    new_string = encode_rdb_length(len(new_value)) + new_value
    tampered = payload[:length_pos] + new_string + payload[data_end:]
    return repatch_crc(tampered)


def craft_bad_module_opcode(payload):
    """Make LoadStringBuffer see the wrong module opcode and re-sign the DUMP."""
    opcode_pos, _, _, _ = module_string_layout(payload)
    tampered = bytearray(payload)
    if tampered[opcode_pos] != 5:
        raise AssertionError("module STRING opcode is not encoded in one byte")
    tampered[opcode_pos] = 1  # RDB_MODULE_OPCODE_SINT
    return repatch_crc(bytes(tampered))


def expect(condition, message):
    if condition:
        print("\x1b[32m✓\x1b[0m %s" % message)
    else:
        print("\x1b[31m✗\x1b[0m %s" % message)
        raise SystemExit(1)


def is_bad_data_error(reply):
    return isinstance(reply, RespError) and "Bad data format" in reply


def main():
    port = int(os.environ.get("REDIS_PORT", "6379"))
    client = RespClient(HOST, port)
    try:
        expect(client.command("PING") == b"PONG", "Server responds to PING")
        expect(client.command("CONFIG", "SET", "rdbcompression", "no") == b"OK",
               "Disable RDB compression so crafted module strings are explicit")

        client.command(
            "DEL",
            "r32_src", "r32_good", "r32_bad", "r32_bad_framing",
            "r64_src", "r64_good", "r64_bad", "r64_bad_framing",
        )
        expect(client.command("R.SETINTARRAY", "r32_src", 0, 1, 2) == b"OK",
               "Seed a small 32-bit bitmap {0, 1, 2}")

        payload32 = client.command("DUMP", "r32_src")
        expect(isinstance(payload32, bytes) and len(payload32) > 10,
               "DUMP returns a serialized 32-bit payload")

        # Self-check: our CRC64 must reproduce the footer Redis wrote for this
        # exact payload. If it does not, every crafted RESTORE below would be
        # rejected on the checksum instead of by the module loader, and the test
        # would pass vacuously -- so assert the match up front.
        recomputed = struct.pack("<Q", crc64(payload32[:-8]))
        expect(recomputed == payload32[-8:],
               "Ported CRC64 matches the DUMP footer Redis computed")

        # A faithfully re-signed but unmodified payload must restore cleanly.
        expect(client.command("RESTORE", "r32_good", 0, repatch_crc(payload32)) == b"OK",
               "Re-signed unmodified 32-bit payload restores successfully")
        expect(client.command("R.GETINTARRAY", "r32_good") == [0, 1, 2],
               "Restored 32-bit bitmap round-trips to {0, 1, 2}")

        # The attack: a huge declared cardinality with a short buffer. The old
        # unsafe deserialize would read ~16 GB past the 17-byte buffer; the safe
        # variant returns NULL and Redis reports "Bad data format".
        huge = craft_oob_payload(payload32, 0xFFFFFFFF)
        reply = client.command("RESTORE", "r32_bad", 0, huge)
        expect(is_bad_data_error(reply),
               "Crafted huge-cardinality payload is rejected with 'Bad data format'")

        # A smaller-but-still-out-of-bounds cardinality must be rejected too.
        moderate = craft_oob_payload(payload32, 1000)
        reply = client.command("RESTORE", "r32_bad", 0, moderate)
        expect(is_bad_data_error(reply),
               "Crafted moderate-cardinality payload is also rejected")

        # Beyond the size check: a payload that is fully in-bounds (so the safe
        # deserialize accepts it) but internally inconsistent -- here an unsorted
        # array container -- must still be rejected by the post-deserialize
        # validation, as the CRoaring maintainer noted on PR #154.
        malformed32 = replace_module_string(payload32, _MALFORMED_R32_BLOB)
        reply = client.command("RESTORE", "r32_bad", 0, malformed32)
        expect(is_bad_data_error(reply),
               "Structurally invalid 32-bit payload is rejected by validation")

        # Redis module framing is outside CRoaring. Without opting in to and
        # checking recoverable module I/O errors, LoadStringBuffer panics Redis
        # when it encounters an unexpected opcode.
        bad_framing32 = craft_bad_module_opcode(payload32)
        reply = client.command("RESTORE", "r32_bad_framing", 0, bad_framing32)
        expect(is_bad_data_error(reply),
               "Malformed 32-bit module framing is rejected without aborting Redis")
        expect(client.command("PING") == b"PONG",
               "Server survives malformed 32-bit module framing")
        expect(client.command("EXISTS", "r32_bad", "r32_bad_framing") == 0,
               "Rejected 32-bit payloads create no keys")

        # Exercise Lemire's requested structural validation independently on the
        # 64-bit loader, including a valid round-trip as a non-regression check.
        expect(client.command("R64.SETINTARRAY", "r64_src", 0, 1, 2) == b"OK",
               "Seed a small 64-bit bitmap {0, 1, 2}")
        payload64 = client.command("DUMP", "r64_src")
        expect(isinstance(payload64, bytes) and len(payload64) > 10,
               "DUMP returns a serialized 64-bit payload")
        expect(struct.pack("<Q", crc64(payload64[:-8])) == payload64[-8:],
               "CRC64 matches the 64-bit DUMP footer")
        expect(client.command("RESTORE", "r64_good", 0, repatch_crc(payload64)) == b"OK",
               "Re-signed unmodified 64-bit payload restores successfully")
        expect(client.command("R64.GETINTARRAY", "r64_good") == [0, 1, 2],
               "Restored 64-bit bitmap round-trips to {0, 1, 2}")

        malformed64 = replace_module_string(payload64, _MALFORMED_R64_BLOB)
        reply = client.command("RESTORE", "r64_bad", 0, malformed64)
        expect(is_bad_data_error(reply),
               "Structurally invalid 64-bit payload is rejected by validation")

        bad_framing64 = craft_bad_module_opcode(payload64)
        reply = client.command("RESTORE", "r64_bad_framing", 0, bad_framing64)
        expect(is_bad_data_error(reply),
               "Malformed 64-bit module framing is rejected without aborting Redis")

        # The server must have survived the crafted loads.
        expect(client.command("PING") == b"PONG", "Server is still alive after the crafted RESTOREs")
        expect(client.command("EXISTS", "r64_bad", "r64_bad_framing") == 0,
               "Rejected 64-bit payloads create no keys")

        print("\nAll integration (6) untrusted deserialization tests passed")
    finally:
        client.close()


if __name__ == "__main__":
    try:
        main()
    except OSError as exc:
        print("integration_6 failed: %s" % exc, file=sys.stderr)
        raise SystemExit(1)
