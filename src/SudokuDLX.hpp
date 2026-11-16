#ifndef SUDOKU_DLX_H
#define SUDOKU_DLX_H

#include <array>
#include <cstdint>
#include <vector>

class SudokuDLX
{
public:
	using Grid = std::array<uint16_t, 81>;

	// 4 constraint types × 81 = 324 columns
	// Row constraint: cell (r,c) has a digit
	// Col constraint: row r has digit d
	// Box constraint: col c has digit d
	// Box constraint: box b has digit d
	static constexpr int COLS = 324;
	static constexpr int ROWS = 729; // 9×9×9 = 729 possible placements

private:
	// DLX node structure
	struct Node {
		Node* left = nullptr;
		Node* right = nullptr;
		Node* up = nullptr;
		Node* down = nullptr;
		Node* column = nullptr;
		int row_id = -1;      // which puzzle row (0-728)
		int constraint_id = -1; // which constraint column (0-323)
		int size = 0;         // column size (only valid for column headers)
	};

public:
	// Draw grid to stdout (same signature as Sudoku::Draw)
	static void Draw(const Grid& grid, bool draw_start = true);
	
	// Solve puzzle using Algorithm X with Dancing Links
	// Returns true if solved, false if no solution
	static bool solve(const Grid& puzzle, Grid& solution);

	// Validate a completed grid
	static bool validate(const Grid& grid);

private:
	// Build the DLX matrix for a given puzzle
	static void buildMatrix(const Grid& puzzle, std::vector<Node>& nodes, Node* headers[COLS]);

	// Cover/uncover column
	static void cover(Node* c);
	static void uncover(Node* c);

	// Search with heuristic (choose column with minimum size)
	static bool search(Node* headers[COLS], std::vector<int>& solution_rows, int depth = 0);

	// Decode solution rows back to grid
	static void decodeSolution(const std::vector<int>& solution_rows, Grid& solution);

	// Constraint column index calculations
	static constexpr int idx(int r, int c) { return r * 9 + c; }
	static constexpr int cellCol(int r, int c) { return idx(r, c); }                    // 0-80
	static constexpr int rowCol(int r, int d) { return 81 + r * 9 + (d - 1); }         // 81-161
	static constexpr int colCol(int c, int d) { return 162 + c * 9 + (d - 1); }        // 162-242
	static constexpr int boxCol(int b, int d) { return 243 + b * 9 + (d - 1); }        // 243-323
	static constexpr int boxOf(int r, int c) { return (r / 3) * 3 + (c / 3); }
};

#endif // SUDOKU_DLX_H