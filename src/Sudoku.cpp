#include <Sudoku.hpp>
#include <iostream>
#include <algorithm>

using Mask = Sudoku::Mask;

Sudoku::Sudoku()
	: _grid{}, _solution{}, _solved(false)
{
	_grid.fill(0);
	_solution.fill(0);
	std::fill(std::begin(_cell_candidates), std::end(_cell_candidates), 0);
}

Sudoku::Sudoku(const Grid& initialGrid)
	: _grid(initialGrid), _solution(initialGrid), _solved(false)
{
	std::fill(std::begin(_cell_candidates), std::end(_cell_candidates), 0);
}

void Sudoku::setGrid(const Grid& grid)
{
	_grid = grid;
	_solution = grid;
	_solved = false;
	std::fill(std::begin(_rows), std::end(_rows), 0);
	std::fill(std::begin(_cols), std::end(_cols), 0);
	std::fill(std::begin(_boxes), std::end(_boxes), 0);
	std::fill(std::begin(_cell_candidates), std::end(_cell_candidates), 0);
}

bool Sudoku::solve()
{
	_solution = _grid;
	std::fill(std::begin(_rows), std::end(_rows), 0);
	std::fill(std::begin(_cols), std::end(_cols), 0);
	std::fill(std::begin(_boxes), std::end(_boxes), 0);

	// Initialize masks from givens
	for (int r = 0; r < N; ++r) {
		for (int c = 0; c < N; ++c) {
			int num = _solution[idx(r, c)];
			if (num != 0) {
				Mask m = DIGIT_MASK[num];
				_rows[r] |= m;
				_cols[c] |= m;
				_boxes[BOX_OF[idx(r, c)]] |= m;
			}
		}
	}

	// Initialize cell candidates
	for (int r = 0; r < N; ++r) {
		for (int c = 0; c < N; ++c) {
			int i = idx(r, c);
			if (_solution[i] == 0) {
				_cell_candidates[i] = _getCandidates(r, c);
			} else {
				_cell_candidates[i] = 0;
			}
		}
	}

	// Apply constraint propagation before search
	_propagate();

	_solved = _solveRecursive();
	return _solved;
}

void Sudoku::Draw(bool draw_start) const
{
	const Grid& grid = draw_start ? _grid : _solution;

	std::cout << "+-------+-------+-------+\n";

	for (int r = 0; r < N; ++r)
	{
		if (r % 3 == 0 && r != 0)
		{
			std::cout << "+-------+-------+-------+\n";
		}

		std::cout << "| ";

		for (int c = 0; c < N; ++c)
		{
			if (c % 3 == 0 && c != 0)
			{
				std::cout << "| ";
			}

			int val = grid[idx(r, c)];
			if (val == 0)
			{
				std::cout << ". ";
			}
			else
			{
				std::cout << val << " ";
			}
		}

		std::cout << "|\n";
	}
	std::cout << "+-------+-------+-------+\n";
}

bool Sudoku::_isValid(int row, int col, int num) const
{
	Mask m = DIGIT_MASK[num];
	int b = BOX_OF[idx(row, col)];
	return !(_rows[row] & m) && !(_cols[col] & m) && !(_boxes[b] & m);
}

Mask Sudoku::_getCandidates(int row, int col) const
{
	int b = BOX_OF[idx(row, col)];
	return ~(_rows[row] | _cols[col] | _boxes[b]) & 0x3FE; // bits 1-9
}

void Sudoku::_updateCandidates(int row, int col)
{
	int i = idx(row, col);
	if (_solution[i] == 0) {
		_cell_candidates[i] = _getCandidates(row, col);
	} else {
		_cell_candidates[i] = 0;
	}
}

int Sudoku::_countBits(Mask m) const
{
	return __builtin_popcount(m);
}

int Sudoku::_findBestCell() const
{
	int bestCell = -1;
	int minCandidates = 10;

	for (int i = 0; i < 81; ++i) {
		if (_solution[i] == 0) {
			Mask candidates = _cell_candidates[i];
			int count = _countBits(candidates);
			if (count < minCandidates) {
				minCandidates = count;
				bestCell = i;
				if (count <= 1) return bestCell;
			}
		}
	}
	return bestCell;
}

// Constraint propagation techniques
bool Sudoku::_propagate()
{
	bool progress = true;
	while (progress) {
		progress = false;
		progress |= _nakedSingles();
		progress |= _hiddenSingles();
	}
	return true;
}

// Naked singles: cell with only 1 candidate
bool Sudoku::_nakedSingles()
{
	bool progress = false;
	for (int i = 0; i < 81; ++i) {
		if (_solution[i] == 0) {
			Mask candidates = _cell_candidates[i];
			if (candidates && (candidates & (candidates - 1)) == 0) {
				int num = __builtin_ctz(candidates);
				int r = i / 9, c = i % 9;
				_solution[i] = num;
				Mask m = DIGIT_MASK[num];
				_rows[r] |= m;
				_cols[c] |= m;
				_boxes[BOX_OF[i]] |= m;
				_cell_candidates[i] = 0;
				progress = true;
			}
		}
	}
	if (progress) {
		for (int r = 0; r < N; ++r) {
			for (int c = 0; c < N; ++c) {
				_updateCandidates(r, c);
			}
		}
	}
	return progress;
}

// Hidden singles: digit appears only once in a house
bool Sudoku::_hiddenSingles()
{
	bool progress = false;

	// Check rows
	for (int r = 0; r < N; ++r) {
		for (int d = 1; d <= 9; ++d) {
			Mask m = DIGIT_MASK[d];
			if (_rows[r] & m) continue;
			int count = 0, last_c = -1;
			for (int c = 0; c < N; ++c) {
				int i = idx(r, c);
				if (_solution[i] == 0 && (_cell_candidates[i] & m)) {
					count++;
					last_c = c;
				}
			}
			if (count == 1) {
				int i = idx(r, last_c);
				_solution[i] = d;
				_rows[r] |= m;
				_cols[last_c] |= m;
				_boxes[BOX_OF[i]] |= m;
				_cell_candidates[i] = 0;
				progress = true;
			}
		}
	}

	// Check columns
	for (int c = 0; c < N; ++c) {
		for (int d = 1; d <= 9; ++d) {
			Mask m = DIGIT_MASK[d];
			if (_cols[c] & m) continue;
			int count = 0, last_r = -1;
			for (int r = 0; r < N; ++r) {
				int i = idx(r, c);
				if (_solution[i] == 0 && (_cell_candidates[i] & m)) {
					count++;
					last_r = r;
				}
			}
			if (count == 1) {
				int i = idx(last_r, c);
				_solution[i] = d;
				_rows[last_r] |= m;
				_cols[c] |= m;
				_boxes[BOX_OF[i]] |= m;
				_cell_candidates[i] = 0;
				progress = true;
			}
		}
	}

	// Check boxes
	for (int b = 0; b < 9; ++b) {
		for (int d = 1; d <= 9; ++d) {
			Mask m = DIGIT_MASK[d];
			if (_boxes[b] & m) continue;
			int count = 0, last_cell = -1;
			for (int i = 0; i < 81; ++i) {
				if (BOX_OF[i] == b && _solution[i] == 0 && (_cell_candidates[i] & m)) {
					count++;
					last_cell = i;
				}
			}
			if (count == 1) {
				int r = last_cell / 9, c = last_cell % 9;
				_solution[last_cell] = d;
				_rows[r] |= m;
				_cols[c] |= m;
				_boxes[b] |= m;
				_cell_candidates[last_cell] = 0;
				progress = true;
			}
		}
	}

	if (progress) {
		for (int r = 0; r < N; ++r) {
			for (int c = 0; c < N; ++c) {
				_updateCandidates(r, c);
			}
		}
	}
	return progress;
}

bool Sudoku::_solveRecursive()
{
	int cell = _findBestCell();
	if (cell == -1) return true; // solved

	int row = cell / 9;
	int col = cell % 9;
	Mask candidates = _cell_candidates[cell];
	int b = BOX_OF[cell];

	// Try each candidate
	while (candidates) {
		Mask m = candidates & -candidates;
		int num = __builtin_ctz(m);
		candidates &= candidates - 1;

		// Place digit
		_solution[cell] = num;
		_rows[row] |= m;
		_cols[col] |= m;
		_boxes[b] |= m;

		// Save and update affected candidates
		Mask saved[81];
		for (int i = 0; i < 81; ++i) saved[i] = _cell_candidates[i];

		for (int r = 0; r < N; ++r) {
			for (int c = 0; c < N; ++c) {
				int i = idx(r, c);
				if (_solution[i] == 0 && (_cell_candidates[i] & m)) {
					_cell_candidates[i] &= ~m;
				}
			}
		}

		if (_solveRecursive())
			return true;

		// Backtrack
		_solution[cell] = 0;
		_rows[row] &= ~m;
		_cols[col] &= ~m;
		_boxes[b] &= ~m;

		// Restore candidates
		for (int i = 0; i < 81; ++i) _cell_candidates[i] = saved[i];
	}

	return false;
}

// Bit-packed grid utilities
Sudoku::PackedGrid Sudoku::packGrid(const Grid& grid)
{
	PackedGrid packed = {};
	for (int i = 0; i < 81; ++i) {
		int bit_pos = i * 4;
		int word = bit_pos / 64;
		int shift = bit_pos % 64;
		packed[word] |= (static_cast<uint64_t>(grid[i]) << shift);
	}
	return packed;
}

Sudoku::Grid Sudoku::unpackGrid(const PackedGrid& packed)
{
	Grid grid = {};
	for (int i = 0; i < 81; ++i) {
		int bit_pos = i * 4;
		int word = bit_pos / 64;
		int shift = bit_pos % 64;
		grid[i] = static_cast<uint16_t>((packed[word] >> shift) & 0xF);
	}
	return grid;
}