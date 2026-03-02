# DOS implementation

This document is intended to store notes on implementation details of the DOS executable `DAVE.EXE`. These observations are mostly gleaned through reverse engineering efforts.

Any references to byte locations in the executable, are based on the decompressed executable. Any references to memory during execution will usually refer to a `DS:_` address, indicating the default data segment set in the program entry point.

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

Once decompressed, levels have a `0x20` byte header followed by a number of planes. All levels in this game only have 2 planes. Other planes may be read but are not interpretted by the game.

### File format

| Offset                    | Size           | Meaning                                          |
| ------------------------- | -------------- | ------------------------------------------------ |
| `0x00`                    | 2              | `uint16_t` width of map in tiles                 |
| `0x02`                    | 2              | `uint16_t` height of map in tiles                |
| `0x04`                    | 2              | `uint16_t` number of planes encoded in file      |
| `0x0E`                    | 2              | Number of bytes per plane (`plane_stride_bytes`) |
| `<0x0E`                   | -              | Other header bytes currently not understood      |
| `0x20`                    | `plane_stride` | First plane data                                 |
| `0x20 + n * plane_stride` | `plane_stride` | `n` plane data                                   |

#### Plane 0 — Tiles

Starting at `0x0E`, the first plane contains sequential tile indexes, grouped into rows then columns. Tiles start from the top left corner of the level.

#### Plane 1 — Tags

Same layout as the first plane. Each word value represents a tag that is interpretted by the game, generally used to spawn entities.

### Coordinates

The game treats the top left corner of the map as `(0,0)` in world coordinates. World coordinates have a finer resolution than pixel coordinates by a factor of 16. So for example, the position at the top left corner of tile `(4, 6)` would be `(64, 96)` in pixel coordinates, and `(1024, 1536)` in world coordinates.

All positional coordinates are unsigned, however directional vectors can be signed. [^1]

The game clamps camera coordinates to a bounding box that has a 2 tile boundary from each map edge.

## Entities

Entities are implemented as 90 byte wide Fat Structs and stored in a pool, with the first entry always being the player. The player struct is always located at `DS:5360` with all other entities being allocated in a pool immediately after. There is a maximum of 164 entities (the first one being the player entity) that can be allocated at a time.

### Entity states

A nice piece of design is the way entity states are managed. Instead of having large functions dealing with all the states an entity can be in there are a number of predefined states that store sprite ids, behaviour and other metadata.

### Sprites

After decompression, sprite files `S_*.DD2` have the following structure:

| Offset | Size             | Meaning                                             |
| ------ | ---------------- | --------------------------------------------------- |
| `0x00` | 2                | `uint16_t` plane/chunk size in bytes (`chunk_size`) |
| `0x02` | `chunk_size * 5` | 5 equal-sized chunks (planes)                       |

- Chunk `0..3`: EGA bitplanes (combined to a 4-bit palette index).
- Chunk `4`: 1bpp transparency mask plane.

Within each of the 5 planes are packed a series of sprites with various widths and heights. These dimensions are not encoded in the file, and must be known ahead of time. The game ships with this data in the executable.

Each sprite is packed at the same offset within each chunk, therefore all chunks are the same size.

#### Sprite metadata

There are 199 sprite records, each with 4 x 32 byte blocks of data starting at `0x125F0`. It's not entirely clear yet why 4 records per sprite are required, it seems to have something to do with the x phase of a sprite in-game and may be technical in nature. For the purposes of decoding sprite data, the first entry of each sprite record should be used.

A sprite ID is just an integer offset into this data, e.g. sprite 7 would start at position `0x125F0 + (32 * 4) * 7`. Sprites are referenced at runtime, and in their state definitions by this ID.

### State definitions

### Entity pooling

## Sound

## Input

[^1]: Needs further validation
