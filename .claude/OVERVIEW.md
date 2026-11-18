## Project Overview

A C++17 Sudoku solver with multiple high-performance algorithms:
- **Bitmask + MRV + Propagation**: Bitmask constraints (uint16_t), Minimum Remaining Values heuristic, constraint propagation (naked/hidden singles), AVX2 vectorization
- **DLX (Algorithm X)**: Dancing Links exact cover implementation
- **Bit-packed grid**: 4-bit per cell storage (48 bytes vs 162 bytes)

Architecture:
- `src/main.cpp` — Benchmark harness with test puzzles
- `src/Sudoku.hpp/cpp` — Bitmask solver with AVX2 optimization
- `src/SudokuDLX.hpp/cpp` — Algorithm X / Dancing Links solver
- `CMakeLists.txt` — Cross-compiler optimization flags (MSVC/Clang/GCC), LTO, PGO support