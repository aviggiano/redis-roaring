# Performance

Tested using CRoaring's `census1881` dataset on the travis build [552223545](https://travis-ci.org/aviggiano/redis-roaring/builds/552223545):

| OP              | TIME/OP (us) | ST.DEV. (us) |
| --------------- | ------------ | ------------ |
| R.SETBIT        | 19.44        | 4.00         |
| R64.SETBIT      | 19.35        | 5.75         |
| SETBIT          | 18.90        | 4.99         |
| R.GETBIT        | 18.74        | 1.28         |
| R64.GETBIT      | 18.80        | 1.25         |
| GETBIT          | 18.40        | 2.31         |
| R.BITCOUNT      | 14.96        | 0.04         |
| R64.BITCOUNT    | 14.76        | 0.03         |
| BITCOUNT        | 38.18        | 0.17         |
| R.BITPOS        | 19.69        | 0.05         |
| R64.BITPOS      | 20.17        | 0.08         |
| BITPOS          | 27.92        | 0.27         |
| R.BITOP NOT     | 38.39        | 0.52         |
| R64.BITOP NOT   | 36.97        | 0.54         |
| BITOP NOT       | 62.43        | 0.32         |
| R.BITOP AND     | 22.95        | 0.10         |
| R64.BITOP AND   | 28.89        | 0.20         |
| BITOP AND       | 163.28       | 2.75         |
| R.BITOP OR      | 27.29        | 0.64         |
| R64.BITOP OR    | 25.09        | 0.16         |
| BITOP OR        | 210.42       | 3.75         |
| R.BITOP XOR     | 23.68        | 0.15         |
| R64.BITOP XOR   | 24.83        | 0.17         |
| BITOP XOR       | 185.91       | 3.24         |
| R.BITOP ANDOR   | 22.97        | 0.09         |
| R64.BITOP ANDOR | 23.45        | 0.11         |
| BITOP ANDOR     | 211.66       | 3.74         |
| R.BITOP ONE     | 23.86        | 0.15         |
| R64.BITOP ONE   | 26.36        | 0.21         |
| BITOP ONE       | 209.89       | 3.70         |
| R.MIN           | 19.63        | 0.03         |
| R64.MIN         | 20.33        | 0.07         |
| MIN             | 20.36        | 0.06         |
| R.MAX           | 19.85        | 0.04         |
| R64.MAX         | 19.49        | 0.03         |
| MAX             | 19.94        | 0.06         |
