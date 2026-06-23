# iec104_apdu_parser

A command-line dialog tool for parsing and validating IEC 60870-5-104 APDUs from hex input.

## Features

- Decodes all three IEC 104 frame types: **I-frame** (Information), **S-frame** (Supervisory), **U-frame** (Unnumbered)
- Fully decodes the ASDU payload of I-frames: type ID, VSQ, COT, OA, CA, and all information objects
- Supports 30+ standard type IDs including measured values, single/double-point, commands, clock synchronisation, and interrogation
- Reports structural errors: wrong start byte, length mismatch, malformed control fields, unknown type IDs
- Configurable application-layer parameters via command-line flags
- Accepts hex input with spaces, colons, or dashes as separators

## Dependencies

- **[lib60870-C](https://github.com/mz-automation/lib60870)** — third-party library, not included in this repository.
  Clone or download it and place it at `vendor/lib60870-C`:
  ```sh
  git clone https://github.com/mz-automation/lib60870.git vendor/lib60870-C
  ```
  The repository contains the C library one level deeper at `lib60870-C/lib60870-C/`, which CMake expects at `vendor/lib60870-C/lib60870-C/` — the clone command above places it correctly.
  ```sh
  ```
- **[cppflags](vendor/cppflags)** — vendored header-only flag parsing library (included).

## Building

Requires CMake 3.10+ and a C++17-capable compiler.

```sh
git clone https://github.com/mz-automation/lib60870.git vendor/lib60870-C
cmake -S . -B build
cmake --build build
```

## Usage

```
./iec104_apdu_parser [flags]
```

### Flags

| Flag | Default | Description |
|---|---|---|
| `--sizeOfCOT` | `2` | Size of Cause of Transmission field (1 = no OA, 2 = with OA) |
| `--originatorAddress` | `0` | Originator address, used when `sizeOfCOT = 2` (0–255) |
| `--sizeOfCA` | `2` | Size of Common Address field in bytes (1 or 2) |
| `--sizeOfIOA` | `3` | Size of Information Object Address field in bytes (1, 2, or 3) |
| `--maxSizeOfASDU` | `249` | Maximum ASDU size in bytes |
| `--unsignedSVA` | | Display SVA (scaled value) fields as unsigned uint16 instead of signed int16 |
| `--help` | | Print usage and exit |

Once started the tool enters an interactive loop. Enter a hex string and press Enter to parse it. An empty line exits.

### Example session

```
$ ./iec104_apdu_parser --sizeOfCA 2 --sizeOfIOA 3

IEC 60870-5-104 APDU Parser
Parameters: sizeOfCOT=2  sizeOfCA=2  sizeOfIOA=3  OA=0
Enter hex bytes (spaces/colons allowed), empty line to quit.
Example: 68 0e 00 00 00 00 64 01 06 00 01 00 00 00 00 14
============================================================

> 68 0e 00 00 00 00 64 01 06 00 01 00 00 00 00 14
------------------------------------------------------------
Raw bytes (16): 68 0E 00 00 00 00 64 01 06 00 01 00 00 00 00 14

Start byte : 0x68  ✓
Length     : 14 (declared)  14 (actual remaining)
APCI ctrl  : 00 00 00 00  (CF1-CF4)

Frame type : I-frame (Information)
  N(S)     : 0
  N(R)     : 0

  ┌─ ASDU ─────────────────────────────────────────
  │  Type ID : 100 (C_IC_NA_1)
  │  VSQ     : numObj=1  SQ=0
  │  COT     : 6 (ACTIVATION)
  │  OA      : 0
  │  CA      : 1
  │  Objects : 1
    [0] IOA=0
        QOI   = 20 (station interrogation)
  └─────────────────────────────────────────────────
============================================================

> 68 04 43 00 00 00
------------------------------------------------------------
Raw bytes (6): 68 04 43 00 00 00

Start byte : 0x68  ✓
Length     : 4 (declared)  4 (actual remaining)
APCI ctrl  : 43 00 00 00  (CF1-CF4)

Frame type : U-frame (Unnumbered)
  Function : TESTFR act
============================================================

>
Goodbye.
```

## APDU structure reference

```
┌────────┬────────┬───────────────────────────────────────────────┐
│  0x68  │ Length │              APCI (4 bytes)                   │
│ 1 byte │ 1 byte │  CF1    CF2    CF3    CF4                     │
└────────┴────────┴───────────────────────────────────────────────┘

I-frame  CF1 bit0=0  N(S) in CF1[7:1]+CF2[7:0], N(R) in CF3[7:1]+CF4[7:0]
S-frame  CF1=0x01    N(R) in CF3[7:1]+CF4[7:0]
U-frame  CF1 bits1:0=11
           STARTDT act=0x07  STARTDT con=0x0B
           STOPDT  act=0x13  STOPDT  con=0x23
           TESTFR  act=0x43  TESTFR  con=0x83
```

## License

Copyright 2026 Pavel Konovalov. Licensed under the GNU General Public License v3.
See [COPYING](COPYING) for the full license text.
