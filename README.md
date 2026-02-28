# Dave 2 port project

This is a project aimed at porting across the classic DOS game Dangerous Dave
2 in the Haunted Mansion to a portable C codebase.

This is mostly an experiment in decompilation, but I'm hopeful to complete this
and have a finished port.

Goals of this project include:

- Aiming for a reimplementation of DD2, not necessarily 1:1 but the game logic
  should be complete and 95% accurate.
- Should be able to compile and run on modern hardware.
- Is not aiming to be compilable on DOS at this stage.

## Running

Users will need to provide the original game assets in the `dos` directory. The
`DAVE.EXE` file is usually compressed with `LZEXE` and needs to be decompressed
using [`UNLZEXE`](https://keenwiki.shikadi.net/wiki/UNLZEXE) manually.

### Windows

```bat
build.bat
```

### macOS (Apple Silicon)

```bash
make
./build/dave2-mac
```

## References

- Data assets decompiled https://github.com/gmegidish/dangerous-dave-re
