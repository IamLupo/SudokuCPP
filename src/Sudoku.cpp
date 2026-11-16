#include <Sudoku.hpp>
#include <iostream>

using Mask = Sudoku::Mask;

Sudoku::Sudoku()
	: _grid{}, _solution{}, _solved(false)
{
	_grid.fill({});
	_solution.fill({});
}

Sudoku::Sudoku(const Grid& initialGrid)
	: _grid(initialGrid), _solution(initialGrid), _solved(false)
{
}

void Sudoku::setGrid(const Grid& grid)
{
	_grid = grid;
	_solution = grid;
	_solved = false;
}

bool Sudoku::solve()
{
	_solution = _grid;
	_rows[0] = _rows[1] = _rows[2] = _rows[3] = _rows[4] = _rows[5] = _rows[6] = _rows[7] = _rows[8] = 0;
	_cols[0] = _cols[1] = _cols[2] = _cols[3] = _cols[4] = _cols[5] = _cols[6] = _cols[7] = _cols[8] = 0;
	_boxes[0] = _boxes[1] = _boxes[2] = _boxes[3] = _boxes[4] = _boxes[5] = _boxes[6] = _boxes[7] = _boxes[8] = 0;

	// Initialize masks from givens
	for (int r = 0; r < N; ++r) {
		for (int c = 0; c < N; ++c) {
			int num = _solution[r][c];
			if (num != 0) {
				Mask m = DIGIT_MASK[num];
				_rows[r] |= m;
				_cols[c] |= m;
				_boxes[BOX_OF[r * 9 + c]] |= m;
			}
		}
	}

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

			if (grid[r][c] == 0)
			{
				std::cout << ". ";
			}
			else
			{
				std::cout << grid[r][c] << " ";
			}
		}

		std::cout << "|\n";
	}
	std::cout << "+-------+-------+-------+\n";
}

bool Sudoku::_isValid(int row, int col, int num) const
{
	Mask m = DIGIT_MASK[num];
	int b = BOX_OF[row * 9 + col];
	return !(_rows[row] & m) && !(_cols[col] & m) && !(_boxes[b] & m);
}

Sudoku::Mask Sudoku::_getCandidates(int row, int col) const
{
	int b = BOX_OF[row * 9 + col];
	return ~(_rows[row] | _cols[col] | _boxes[b]) & 0x3FE; // bits 1-9
}

int Sudoku::_countBits(Mask m) const
{
	return __builtin_popcount(m);
}

int Sudoku::_findBestCell() const
{
	int bestCell = -1;
	int minCandidates = 10;

	for (int r = 0; r < N; ++r)
	{
		for (int c = 0; c < N; ++c)
		{
			if (_solution[r][c] == 0)
			{
				Mask candidates = _getCandidates(r, c);
				int count = _countBits(candidates);
				
				if (count < minCandidates)
				{
					minCandidates = count;
					bestCell = r * 9 + c;

					if (count == 1)
						return bestCell; // early exit
				}
			}
		}
	}
	
	return bestCell;
}

bool Sudoku::_solveRecursive()
{
	int cell = _findBestCell();
	if (cell == -1) return true; // solved

	int row = cell / 9;
	int col = cell % 9;
	Mask candidates = _getCandidates(row, col);
	int b = BOX_OF[cell];

	// Try each candidate
	while (candidates) {
		Mask m = candidates & -candidates; // isolate lowest bit
		int num = __builtin_ctz(m); // get digit (1-9)
		candidates &= candidates - 1; // clear lowest bit

		// Place digit
		_solution[row][col] = num;
		_rows[row] |= m;
		_cols[col] |= m;
		_boxes[b] |= m;

		if (_solveRecursive())
			return true;

		// Backtrack
		_solution[row][col] = 0;
		_rows[row] &= ~m;
		_cols[col] &= ~m;
		_boxes[b] &= ~m;
	}

	return false;
}