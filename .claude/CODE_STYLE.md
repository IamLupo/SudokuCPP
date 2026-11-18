## Code Style

(Allman brace style):
- Opening braces on new line for `if`, `for`, `while`, `else`, functions
- Braces aligned at same indentation level
- Class order: forward type declarations → static members → member variables → public interface → private helpers

**Sudoku class (bitmask solver):**
- `Grid` = `std::array<uint16_t, 81>` (flat, 162 bytes)
- `Mask` = `uint16_t` (bits 1-9 = candidates)
- `PackedGrid` = `std::array<uint64_t, 6>` (48 bytes, 4 bits/cell)
- `EXAMPLE_PUZZLE`, `HARD_PUZZLE`, etc. — `constexpr` static puzzles
- Precomputed lookup tables: `PEERS[81][20]`, `BOX_CELLS[9][9]`, `BOX_OF[81]`, `DIGIT_MASK[10]`
- AVX2 vectorization for MRV, naked singles, candidate clearing
- Constraint propagation (naked/hidden singles) before/during search
- `Draw(bool draw_start)` — renders grid (`.` for empty cells)

**SudokuDLX class (Algorithm X):**
- Fixed node buffer (no heap allocation)
- 324 columns × 729 rows exact cover matrix
- Knuth's S heuristic (minimum column size)

## Key Design Decisions

1. **Member variables early** — declared in `private:` before public interface
2. **Leading underscore** — `_grid`, `_solution`, `_solved` for private members
3. **`constexpr` puzzles** — zero runtime overhead, stored in `.rodata`
4. **Flat grid + bitmask** — O(1) constraint checking, cache-friendly
5. **Precomputed tables** — `PEERS`, `BOX_CELLS`, `BOX_OF` computed at compile time
6. **AVX2 intrinsics** — vectorized popcount, MRV, naked singles (16 lanes)
7. **Angle-bracket includes** — `#include <Sudoku.hpp>` via `target_include_directories`