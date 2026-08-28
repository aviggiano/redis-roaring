#!/usr/bin/env python3
"""Regression test for the unbounded OOB read in BitmapRdbLoad (issue #153).

src/r_32.c used to deserialize an RDB/RESTORE payload with the unsafe
roaring_bitmap_deserialize(), which trusts the serialized cardinality header
and reads that many uint32 elements regardless of the actual buffer length. A
crafted RESTORE payload (any authenticated client can send one) whose
ARRAY_UINT32 header declares a huge cardinality therefore drove a large
out-of-bounds read. The fix switches to roaring_bitmap_deserialize_safe(), which
bounds every read by the loaded size and returns NULL on a truncated or corrupt
payload -- Redis then rejects the RESTORE with "Bad data format" and stays up.

This test crafts exactly that payload by taking a valid DUMP of a small bitmap,
inflating the cardinality field in place, recomputing Redis's CRC64 footer so
the payload passes RESTORE's checksum check and actually reaches the module
loader, and asserting the server rejects it cleanly instead of crashing.

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
_CARD_OFFSET_IN_SIG = 1  # cardinality field starts one byte into the signature


def repatch_crc(payload):
    """Return payload with its trailing 8-byte CRC64 footer recomputed."""
    body = payload[:-8]
    return body + struct.pack("<Q", crc64(body))


def craft_oob_payload(payload, fake_cardinality):
    """Inflate the ARRAY_UINT32 cardinality in a valid DUMP and re-sign it."""
    pos = payload.find(_ARRAY_SIG)
    if pos < 0:
        raise AssertionError(
            "ARRAY_UINT32 signature for {0,1,2} not found in the DUMP payload; "
            "the 32-bit serialization format may have changed -- update this test"
        )
    card_at = pos + _CARD_OFFSET_IN_SIG
    tampered = bytearray(payload)
    tampered[card_at:card_at + 4] = struct.pack("<I", fake_cardinality)
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

        client.command("DEL", "oob_src", "oob_good", "oob_bad")
        expect(client.command("R.SETINTARRAY", "oob_src", 0, 1, 2) == b"OK",
               "Seed a small 32-bit bitmap {0, 1, 2}")

        payload = client.command("DUMP", "oob_src")
        expect(isinstance(payload, bytes) and len(payload) > 10,
               "DUMP returns a serialized payload")

        # Self-check: our CRC64 must reproduce the footer Redis wrote for this
        # exact payload. If it does not, every crafted RESTORE below would be
        # rejected on the checksum instead of by the module loader, and the test
        # would pass vacuously -- so assert the match up front.
        recomputed = struct.pack("<Q", crc64(payload[:-8]))
        expect(recomputed == payload[-8:],
               "Ported CRC64 matches the DUMP footer Redis computed")

        # A faithfully re-signed but unmodified payload must restore cleanly.
        expect(client.command("RESTORE", "oob_good", 0, repatch_crc(payload)) == b"OK",
               "Re-signed unmodified payload restores successfully")
        expect(client.command("R.GETINTARRAY", "oob_good") == [0, 1, 2],
               "Restored bitmap round-trips to {0, 1, 2}")

        # The attack: a huge declared cardinality with a short buffer. The old
        # unsafe deserialize would read ~16 GB past the 17-byte buffer; the safe
        # variant returns NULL and Redis reports "Bad data format".
        huge = craft_oob_payload(payload, 0xFFFFFFFF)
        reply = client.command("RESTORE", "oob_bad", 0, huge)
        expect(is_bad_data_error(reply),
               "Crafted huge-cardinality payload is rejected with 'Bad data format'")

        # A smaller-but-still-out-of-bounds cardinality must be rejected too.
        moderate = craft_oob_payload(payload, 1000)
        reply = client.command("RESTORE", "oob_bad", 0, moderate)
        expect(is_bad_data_error(reply),
               "Crafted moderate-cardinality payload is also rejected")

        # The server must have survived the crafted loads.
        expect(client.command("PING") == b"PONG", "Server is still alive after the crafted RESTOREs")
        expect(client.command("EXISTS", "oob_bad") == 0, "No key was created from the rejected payloads")

        print("\nAll integration (6) OOB deserialization tests passed")
    finally:
        client.close()


if __name__ == "__main__":
    try:
        main()
    except OSError as exc:
        print("integration_6 failed: %s" % exc, file=sys.stderr)
        raise SystemExit(1)
