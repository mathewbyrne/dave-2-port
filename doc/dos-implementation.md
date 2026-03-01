# DOS implementation

This document is intended to store notes on implementation details of the DOS executable `DAVE.EXE`. These observations are mostly gleaned through reverse engineering efforts.

## Data encoding

The main executable is encoded using `LZEXE`, specifically `LZ91` and can be decoded using `UNLZEXE`.

There are two generic methods of compression used in asset files, Huffman encoding, and Run length encoding.

### Huffman encoding

Identified by a `HUFF` 4 byte header.

The Huffman container format has the following structure:

| Offset  | Size    | Meaning                                |
| ------- | ------- | -------------------------------------- |
| `0x00`  | 4       | ASCII magic: `HUFF`                    |
| `0x04`  | 4       | `uint32_t` decoded length (bytes)      |
| `0x08`  | `0x3FC` | Node table (`255` nodes, 4 bytes each) |
| `0x404` | ...     | Bitstream payload                      |

Node table entries are two little-endian `uint16_t` values: `bit0` and `bit1`.

- Values `< 0x100` are literal output bytes.
- Values `>= 0x100` are internal-node references, where node index is `value - 0x100`.
- Decoding starts from node index `254` (the root).
- Bits are consumed LSB-first within each payload byte (`1 << 0`, then `1 << 1`, ...).
- The stream has no explicit end marker; decoding stops once the output byte count reaches the decoded length from offset `0x04`.

### Run length encoding

Word-based Run Length Encoding (RLEW) is implemented for level files (`LEVEL*.DD2`).

There isn't any specific marker for this encoding type.

The first 4 bytes are a `uint32_t` indicating the decoded length in bytes. The rest of the file is comprised of a sequence of words. `0xFEFE` is a marker indicating a run-length triplet, with the first word the special marker, the second a `uint16_t` count, and a 2 byte value.

### Encoding by file

| Filename     | Encoding |
| ------------ | -------- |
| CTLPANEL.DD2 | None     |
| EGATILES.DD2 | None     |
| INTRO.DD2    | None     |
| LEVEL01.DD2  | RLEW     |
| LEVEL02.DD2  | RLEW     |
| LEVEL03.DD2  | RLEW     |
| LEVEL04.DD2  | RLEW     |
| LEVEL05.DD2  | RLEW     |
| LEVEL06.DD2  | RLEW     |
| LEVEL07.DD2  | RLEW     |
| LEVEL08.DD2  | RLEW     |
| PROGPIC.DD2  | HUFF     |
| S_CHUNK1.DD2 | HUFF     |
| S_CHUNK2.DD2 | HUFF     |
| S_DAVE.DD2   | HUFF     |
| S_FRANK.DD2  | HUFF     |
| S_MASTER.DD2 | HUFF     |
| STARPIC.DD2  | HUFF     |
| TITLE1.DD2   | None     |
| TITLE2.DD2   | None     |

## Levels

## Entities

### Sprites

After decompression, sprite files `S_*.DD2` have the following structure:

| Offset | Size             | Meaning                                             |
| ------ | ---------------- | --------------------------------------------------- |
| `0x00` | 2                | `uint16_t` plane/chunk size in bytes (`chunk_size`) |
| `0x02` | `chunk_size * 5` | 5 equal-sized chunks (planes)                       |

Plane meaning:

- Chunk `0..3`: EGA bitplanes (combined to a 4-bit palette index).
- Chunk `4`: 1bpp transparency mask plane.

Within each of the 5 planes are packed a series of sprites with various widths and heights. These dimensions are not encoded in the file, and must be known ahead of time. The game ships with this data in the executable.

Each sprite is packed at the same offset within each chunk, therefore all chunks are the same size.

### State definitions

### Entity pooling

## Sound

## Input
