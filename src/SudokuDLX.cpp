#include "SudokuDLX.hpp"
#include <algorithm>
#include <climits>
#include <iostream>

bool SudokuDLX::solve(const Grid& puzzle, Grid& solution)
{
	// Validate input
	for (int i = 0; i < 81; ++i)
	{
		int v = puzzle[i];
		if (v < 0 || v > 9) return false;
	}

	// Pre-allocate nodes: 1 header per column + up to 4 nodes per row × 729 rows
	std::vector<Node> nodes;
	nodes.reserve(COLS + ROWS * 4);

	// Create column headers
	Node* headers[COLS] = {};
	for (int i = 0; i < COLS; ++i)
	{
		nodes.emplace_back();
		headers[i] = &nodes.back();
		headers[i]->constraint_id = i;
		headers[i]->size = 0;
		headers[i]->up = headers[i]->down = headers[i];
	}

	// Link headers horizontally
	for (int i = 0; i < COLS; ++i)
	{
		headers[i]->right = headers[(i + 1) % COLS];
		headers[i]->left = headers[(i - 1 + COLS) % COLS];
	}

	buildMatrix(puzzle, nodes, headers);

	std::vector<int> solution_rows;
	solution_rows.reserve(81);

	if (search(headers, solution_rows))
	{
		decodeSolution(solution_rows, solution);

		return true;
	}

	return false;
}

void SudokuDLX::buildMatrix(const Grid& puzzle, std::vector<Node>& nodes, Node* headers[COLS])
{
	// For each possible placement (r, c, d)
	for (int r = 0; r < 9; ++r)
	{
		for (int c = 0; c < 9; ++c)
		{
			for (int d = 1; d <= 9; ++d)
			{
				// If puzzle has a given, only allow that digit
				if (puzzle[idx(r, c)] != 0 && puzzle[idx(r, c)] != d)
					continue;

				int row_id = r * 81 + c * 9 + (d - 1);
				int b = boxOf(r, c);

				// Four constraints for this placement
				int cols[4] = {
					cellCol(r, c),      // cell (r,c) filled
					rowCol(r, d),       // row r has digit d
					colCol(c, d),       // col c has digit d
					boxCol(b, d)        // box b has digit d
				};

				Node* first = nullptr;
				Node* prev = nullptr;

				for (int ci = 0; ci < 4; ++ci)
				{
					int col_idx = cols[ci];
					nodes.emplace_back();
					Node* node = &nodes.back();
					node->row_id = row_id;
					node->constraint_id = col_idx;
					node->column = headers[col_idx];

					// Link vertically (at bottom of column)
					node->up = headers[col_idx]->up;
					node->down = headers[col_idx];
					headers[col_idx]->up->down = node;
					headers[col_idx]->up = node;
					headers[col_idx]->size++;

					// Link horizontally
					if (first == nullptr)
					{
						first = node;
						prev = node;
						node->left = node->right = node;
					}
					else
					{
						node->left = prev;
						node->right = first;
						prev->right = node;
						first->left = node;
						prev = node;
					}
				}
			}
		}
	}
}

void SudokuDLX::cover(Node* c)
{
	// Remove column header from horizontal list
	c->right->left = c->left;
	c->left->right = c->right;

	// Remove all rows in this column from other columns
	for (Node* i = c->down; i != c; i = i->down)
	{
		for (Node* j = i->right; j != i; j = j->right)
		{
			j->down->up = j->up;
			j->up->down = j->down;
			j->column->size--;
		}
	}
}

void SudokuDLX::uncover(Node* c)
{
	// Restore rows in reverse order
	for (Node* i = c->up; i != c; i = i->up)
	{
		for (Node* j = i->left; j != i; j = j->left)
		{
			j->column->size++;
			j->down->up = j;
			j->up->down = j;
		}
	}

	// Restore column header to horizontal list
	c->right->left = c;
	c->left->right = c;
}

bool SudokuDLX::search(Node* headers[COLS], std::vector<int>& solution_rows, int depth)
{
	// If header points to itself, all columns covered = solved
	if (headers[0]->right == headers[0])
	{
		return true;
	}

	// Choose column with minimum size (Knuth's S heuristic)
	Node* best_col = nullptr;
	int min_size = INT_MAX;

	for (Node* c = headers[0]->right; c != headers[0]; c = c->right)
	{
		if (c->size < min_size)
		{
			min_size = c->size;
			best_col = c;

			if (min_size == 0)
				return false; // dead end

			if (min_size == 1)
				break; // optimal
		}
	}

	if (!best_col || min_size == 0)
		return false;

	cover(best_col);

	// Try each row in this column
	for (Node* r = best_col->down; r != best_col; r = r->down)
	{
		solution_rows.push_back(r->row_id);

		// Cover all other columns in this row
		for (Node* j = r->right; j != r; j = j->right)
		{
			cover(j->column);
		}

		if (search(headers, solution_rows, depth + 1))
			return true;

		// Backtrack
		solution_rows.pop_back();
		for (Node* j = r->left; j != r; j = j->left)
		{
			uncover(j->column);
		}
	}

	uncover(best_col);
	return false;
}

void SudokuDLX::decodeSolution(const std::vector<int>& solution_rows, Grid& solution)
{
	solution.fill(0);
	for (int row_id : solution_rows)
	{
		int r = row_id / 81;
		int c = (row_id % 81) / 9;
		int d = (row_id % 9) + 1;
		solution[idx(r, c)] = d;
	}
}

bool SudokuDLX::validate(const Grid& grid)
{
	// Check rows
	for (int r = 0; r < 9; ++r)
	{
		uint16_t seen = 0;

		for (int c = 0; c < 9; ++c)
		{
			int v = grid[idx(r, c)];

			if (v < 1 || v > 9)
				return false;
			
			if (seen & (1 << v))
				return false;
			
			seen |= 1 << v;
		}
	}

	// Check columns
	for (int c = 0; c < 9; ++c)
	{
		uint16_t seen = 0;

		for (int r = 0; r < 9; ++r)
		{
			int v = grid[idx(r, c)];

			if (seen & (1 << v))
				return false;
			
			seen |= 1 << v;
		}
	}

	// Check boxes
	for (int br = 0; br < 3; ++br)
	{
		for (int bc = 0; bc < 3; ++bc)
		{
			uint16_t seen = 0;

			for (int r = 0; r < 3; ++r)
			{
				for (int c = 0; c < 3; ++c)
				{
					int v = grid[idx(br * 3 + r, bc * 3 + c)];

					if (seen & (1 << v))
						return false;

					seen |= 1 << v;
				}
			}
		}
	}

	return true;
}

void SudokuDLX::Draw(const Grid& grid, bool draw_start)
{
	(void)draw_start; // unused for DLX, but kept for API compatibility

	std::cout << "+-------+-------+-------+\n";
	for (int r = 0; r < 9; ++r) {
		if (r % 3 == 0 && r != 0) {
			std::cout << "+-------+-------+-------+\n";
		}
		std::cout << "| ";
		for (int c = 0; c < 9; ++c) {
			if (c % 3 == 0 && c != 0) {
				std::cout << "| ";
			}
			int v = grid[idx(r, c)];
			if (v == 0) std::cout << ". ";
			else std::cout << v << ' ';
		}
		std::cout << "|\n";
	}
	std::cout << "+-------+-------+-------+\n";
}
