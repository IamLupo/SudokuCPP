#ifndef SUDOKU_H
#define SUDOKU_H

#include <array>
#include <cstdint>
#include <string>

class Sudoku
{
public:
	// 1. Forward type declarations
	using Grid = std::array<std::array<int, 9>, 9>;
	static constexpr int N = 9;
	using Mask = uint16_t;  // bits 1-9 = candidates

	// 2. Static data members
	static constexpr Grid EXAMPLE_PUZZLE = {{
		{5, 3, 0, 0, 7, 0, 0, 0, 0},
		{6, 0, 0, 1, 9, 5, 0, 0, 0},
		{0, 9, 8, 0, 0, 0, 0, 6, 0},

		{8, 0, 0, 0, 6, 0, 0, 0, 3},
		{4, 0, 0, 8, 0, 3, 0, 0, 1},
		{7, 0, 0, 0, 2, 0, 0, 0, 6},

		{0, 6, 0, 0, 0, 0, 2, 8, 0},
		{0, 0, 0, 4, 1, 9, 0, 0, 5},
		{0, 0, 0, 0, 8, 0, 0, 7, 9}
	}};

	// Precomputed lookup tables
	static constexpr Mask DIGIT_MASK[10] = {
		0,          // 0 (unused)
		1 << 1,     // 1
		1 << 2,     // 2
		1 << 3,     // 3
		1 << 4,     // 4
		1 << 5,     // 5
		1 << 6,     // 6
		1 << 7,     // 7
		1 << 8,     // 8
		1 << 9      // 9
	};

	static constexpr int BOX_OF[81] = {
		0,0,0, 1,1,1, 2,2,2,
		0,0,0, 1,1,1, 2,2,2,
		0,0,0, 1,1,1, 2,2,2,

		3,3,3, 4,4,4, 5,5,5,
		3,3,3, 4,4,4, 5,5,5,
		3,3,3, 4,4,4, 5,5,5,
		
		6,6,6, 7,7,7, 8,8,8,
		6,6,6, 7,7,7, 8,8,8,
		6,6,6, 7,7,7, 8,8,8
	};

private:
	// Member variables (early visibility of state)
	Grid _grid;
	Grid _solution;
	bool _solved;

	// Bitmask state for O(1) constraint checking
	Mask _rows[9] = {};
	Mask _cols[9] = {};
	Mask _boxes[9] = {};

public:
	// 3. Constructors
	Sudoku();
	explicit Sudoku(const Grid& initialGrid);

	// 4. Public interface
	void setGrid(const Grid& grid);

	Grid getGrid() const { return _grid; }
	Grid getSolution() const { return _solution; }

	bool solve();
	bool isSolved() const { return _solved; }

	void Draw(bool draw_start = true) const;

private:
	// Private helper functions
	bool _isValid(int row, int col, int num) const;
	Mask _getCandidates(int row, int col) const;
	int _countBits(Mask m) const;
	int _findBestCell() const;
	bool _solveRecursive();
};

#endif // SUDOKU_H