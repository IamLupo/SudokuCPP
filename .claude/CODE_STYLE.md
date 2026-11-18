## Code Style

(Allman brace style):
- Opening braces on new line for `if`, `for`, `while`, `else`, functions
- Braces aligned at same indentation level
- If you create a class use forward declaraitions first, then variable members and then functions definitions

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