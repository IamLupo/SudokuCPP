# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@SOUL.md

## Project Overview

A C++17 Sudoku solver with a clean class-based architecture. Uses recursive backtracking to solve 9x9 Sudoku puzzles.

## Build Commands

```bash
# Configure and build
cmake -B build -S .
cmake --build build

# Run
./build/hello.exe
```



**Sudoku class:**
- `Grid` = `std::array<std::array<int, 9>, 9>` (two copies: `_grid` + `_solution`)
- `EXAMPLE_PUZZLE` — `constexpr` static puzzle data (compile-time constant)
- Recursive backtracking solver with position-tracking optimization
- `Draw(bool draw_start)` — renders grid (`.` for empty cells)

## Key Design Decisions

1. **Member variables early** — declared in `private:` section before public interface for visible memory layout
2. **Leading underscore** — `_grid`, `_solution`, `_solved` for private members
3. **`constexpr` puzzle** — zero runtime overhead, stored in `.rodata`
4. **Single-pass recursion** — `solveRecursive(grid, row, col)` continues from last position, no `findEmpty` scan
5. **Angle-bracket includes** — `#include <Sudoku.hpp>` via `target_include_directories(hello PRIVATE src)`