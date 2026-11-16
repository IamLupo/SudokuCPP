#include <iostream>

#include <Sudoku.hpp>

int main()
{
	Sudoku sudoku(Sudoku::EXAMPLE_PUZZLE);

	std::cout << "START SUDOKU:" << std::endl;
	sudoku.Draw(true);

	if (sudoku.solve())
	{
		std::cout << "SOLVED SUDOKU:" << std::endl;
		sudoku.Draw(false);
	}
	else
	{
		std::cout << "No solution exists!\n";
	}

	return 0;
}