# Repo Layout Changelog (2026-03-05)

This file records repository structure changes made to align BinFontLib with common Arduino library conventions.

## Summary

### Examples
- Re-generated `examples/M5PaperS3_Demo/M5PaperS3_Demo.ino` to be a minimal entry that delegates to `BinTestApp_setup()` / `BinTestApp_loop()`.
- Added `BinTestApp_Shared.cpp` shims in examples to reuse the single implementation in `examples/_shared/BinTestApp.cpp` (avoid duplicated demo logic).
- Created a new example `examples/BinFontLib/` as the replacement for the previous root-level sketch.
- Added `partitions.csv` into the relevant example folders to support `PartitionScheme=custom` builds.

### Root Cleanup
- Moved all root-level `*.md` documentation files into the `readme/` directory.
- Moved development/build helper content into `extras/`:
  - `build/` -> `extras/build/`
  - `platforms/` -> `extras/platforms/`
  - `github_init.py` -> `extras/tools/github_init.py`
  - `partitions.csv` -> `extras/partitions.csv`
- Removed non-standard root-level `.ino` sketch (now under `examples/`).
- Removed root-level wrapper headers (`BinFontLib.h`, `BinFontConfig.h`, `BinTestApp.h`) so public headers live under `src/`.
- Added `README.txt` in root to point to the moved documentation.

## Notes
- For Arduino usage, `#include <BinFontLib.h>` resolves to the header under `src/`.
- `extras/` is intended for development assets; Arduino builds should only compile sources under `src/`.
