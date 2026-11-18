#ifndef SUDOKU_H
#define SUDOKU_H

#include <array>
#include <cstdint>
#include <string>

class Sudoku
{
public:
	// 1. Forward type declarations
	// Flat grid: 81 cells × uint16_t (0-9) = 162 bytes (vs 324 for nested array)
	using Grid = std::array<uint16_t, 81>;
	static constexpr int N = 9;
	using Mask = uint16_t;  // bits 1-9 = candidates

	// Fully bit-packed grid: 81 cells × 4 bits = 324 bits = 6 × uint64_t (48 bytes)
	using PackedGrid = std::array<uint64_t, 6>;

	// Static utility functions
	static constexpr int idx(int r, int c) { return r * 9 + c; }

	// Precomputed lookup tables (compile-time initialized via lambdas)
	static constexpr auto PEERS = []() {
		std::array<std::array<int, 20>, 81> p{};
		for (int cell = 0; cell < 81; ++cell) {
			const int row = cell / 9;
			const int col = cell % 9;
			const int box = (row / 3) * 3 + (col / 3);
			int count = 0;
			for (int other = 0; other < 81; ++other) {
				if (other == cell) continue;
				const int otherRow = other / 9;
				const int otherCol = other % 9;
				const int otherBox = (otherRow / 3) * 3 + (otherCol / 3);
				if (otherRow == row || otherCol == col || otherBox == box) {
					p[cell][count++] = other;
				}
			}
		}
		return p;
	}();

	static constexpr auto BOX_CELLS = []() {
		std::array<std::array<int, 9>, 9> b{};
		for (int box = 0; box < 9; ++box) {
			int count = 0;
			for (int cell = 0; cell < 81; ++cell) {
				const int row = cell / 9;
				const int col = cell % 9;
				if ((row / 3) * 3 + (col / 3) == box) {
					b[box][count++] = cell;
				}
			}
		}
		return b;
	}();

	// 2. Static data members
	static constexpr Grid EXAMPLE_PUZZLE = {
		5,3,0, 0,7,0, 0,0,0,
		6,0,0, 1,9,5, 0,0,0,
		0,9,8, 0,0,0, 0,6,0,

		8,0,0, 0,6,0, 0,0,3,
		4,0,0, 8,0,3, 0,0,1,
		7,0,0, 0,2,0, 0,0,6,

		0,6,0, 0,0,0, 2,8,0,
		0,0,0, 4,1,9, 0,0,5,
		0,0,0, 0,8,0, 0,7,9
	};

	// Known hard puzzle (AI Escargot / "Easter Monster" variant)
	static constexpr Grid HARD_PUZZLE = {
		0,0,0, 0,0,0, 0,0,0,
		0,0,0, 0,0,3, 0,8,5,
		0,0,1, 0,2,0, 0,0,0,

		0,0,0, 5,0,7, 0,0,0,
		0,0,4, 0,0,0, 1,0,0,
		0,9,0, 0,0,0, 0,0,0,

		5,0,0, 0,0,0, 0,7,3,
		0,0,2, 0,1,0, 0,0,0,
		0,0,0, 0,4,0, 0,0,9
	};

	// Additional test puzzles for correctness verification
	static constexpr Grid DEEP_BACKTRACK = {{
		0,0,5, 3,0,0, 0,0,0,
		8,0,0, 0,0,0, 0,2,0,
		0,7,0, 0,1,0, 5,0,0,
		4,0,0, 0,0,5, 3,0,0,
		0,1,0, 0,7,0, 0,0,6,
		0,0,3, 2,0,0, 0,8,0,
		0,6,0, 5,0,0, 0,0,9,
		0,0,4, 0,0,0, 0,3,0,
		0,0,0, 0,0,9, 7,0,0
	}};

	static constexpr Grid PUZZLE_17_CLUES = {{
		0,0,0, 0,0,0, 0,0,0,
		0,0,0, 0,0,3, 0,8,5,
		0,0,1, 0,2,0, 0,0,0,
		0,0,0, 5,0,7, 0,0,0,
		0,0,4, 0,0,0, 1,0,0,
		0,9,0, 0,0,0, 0,0,0,
		5,0,0, 0,0,0, 0,7,3,
		0,0,2, 0,1,0, 0,0,0,
		0,0,0, 0,4,0, 0,0,9
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

	// Bit-packed candidates for each cell (Mask = uint16_t = 16 bits, bits 1-9 used)
	// 81 cells × 16 bits = 162 bytes
	Mask _cell_candidates[81];

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

	// Cell access helpers
	int get(int r, int c) const { return _grid[idx(r, c)]; }
	int getSolution(int r, int c) const { return _solution[idx(r, c)]; }

	// Static utility: pack/unpack grid
	static PackedGrid packGrid(const Grid& grid);
	static Grid unpackGrid(const PackedGrid& packed);

private:
	// Private helper functions
	Mask _getCandidates(int row, int col) const;
	void _updateCandidates(int row, int col);
	int _countBits(Mask m) const;
	int _findBestCell() const;

	// Constraint propagation
	bool _propagate();
	bool _nakedSingles();
	bool _hiddenSingles();

	// Recursive solver with propagation
	bool _solveRecursive();
};

#endif // SUDOKU_H