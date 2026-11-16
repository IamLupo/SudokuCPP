#include <iostream>
#include <chrono>
#include <Sudoku.hpp>
#include <SudokuDLX.hpp>

using Grid = Sudoku::Grid;
using Clock = std::chrono::high_resolution_clock;

template<typename Solver>
void benchmark(const char* name, const Grid& puzzle, Grid& solution)
{
	auto start = Clock::now();
	bool solved = Solver::solve(puzzle, solution);
	auto end = Clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

	std::cout << name << ": ";
	if (solved)
	{
		std::cout << "SOLVED in " << duration.count() << " µs\n";
	}
	else
	{
		std::cout << "FAILED\n";
	}
}

// Known hard puzzle (AI Escargot / "Easter Monster" variant)
static const Grid HARD_PUZZLE = {{
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

int main()
{
	std::cout << "Starting...\n";
	Grid puzzle = Sudoku::EXAMPLE_PUZZLE;
	Grid solution;

	std::cout << "=== Sudoku Solver Benchmark ===\n\n";

	// Draw starting puzzle
	SudokuDLX::Draw(puzzle, true);
	std::cout << "\n";

	// Test bitmask + MRV + propagation solver
	{
		Sudoku s(puzzle);
		s.solve();
		s.Draw(false);  // false = draw solution
	}

	std::cout << "\n---\n";

	// Test DLX solver
	benchmark<SudokuDLX>("DLX (Algorithm X)", puzzle, solution);
	if (SudokuDLX::validate(solution))
	{
		SudokuDLX::Draw(solution, false);
	}

	std::cout << "\n---\n";

	// Benchmark multiple runs (use example puzzle which is easier)
	std::cout << "\n=== Performance Test (100 iterations on example puzzle) ===\n";

	// Warmup
	for (int i = 0; i < 5; ++i)
	{
		Sudoku s(puzzle);
		s.solve();
		SudokuDLX::solve(puzzle, solution);
	}

	// Benchmark bitmask solver
	{
		auto start = Clock::now();
		for (int i = 0; i < 100; ++i)
		{
			Sudoku s(puzzle);
			s.solve();
		}
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "Bitmask + MRV + Propagation: "
			<< duration.count() / 100.0 << " µs avg\n";
	}

	// Benchmark DLX
	{
		auto start = Clock::now();
		for (int i = 0; i < 100; ++i)
		{
			SudokuDLX::solve(puzzle, solution);
		}
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "DLX (Algorithm X): "
			<< duration.count() / 100.0 << " µs avg\n";
	}

	// Test with hard puzzle (requires more backtracking)
	std::cout << "\n=== Hard Puzzle Test (AI Escargot) ===\n";
	Grid hard_solution;

	SudokuDLX::Draw(HARD_PUZZLE, true);
	std::cout << "\n";

	{
		Sudoku s(HARD_PUZZLE);
		auto start = Clock::now();
		bool solved = s.solve();
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "Bitmask + MRV + Propagation: ";
		if (solved) {
			std::cout << "SOLVED in " << duration.count() << " µs\n";
			s.Draw(false);
		} else {
			std::cout << "FAILED\n";
		}
	}

	std::cout << "\n---\n";

	{
		auto start = Clock::now();
		bool solved = SudokuDLX::solve(HARD_PUZZLE, hard_solution);
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "DLX (Algorithm X): ";
		if (solved) {
			std::cout << "SOLVED in " << duration.count() << " µs\n";
			SudokuDLX::Draw(hard_solution, false);
		} else {
			std::cout << "FAILED\n";
		}
	}

	// Benchmark hard puzzle multiple runs
	std::cout << "\n=== Hard Puzzle Benchmark (50 iterations) ===\n";

	// Warmup
	for (int i = 0; i < 3; ++i) {
		Sudoku s(HARD_PUZZLE);
		s.solve();
		SudokuDLX::solve(HARD_PUZZLE, hard_solution);
	}

	// Benchmark bitmask solver on hard puzzle
	{
		auto start = Clock::now();
		for (int i = 0; i < 50; ++i)
		{
			Sudoku s(HARD_PUZZLE);
			s.solve();
		}
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "Bitmask + MRV + Propagation: "
			<< duration.count() / 50.0 << " µs avg\n";
	}

	// Benchmark DLX on hard puzzle
	{
		auto start = Clock::now();
		for (int i = 0; i < 50; ++i)
		{
			SudokuDLX::solve(HARD_PUZZLE, hard_solution);
		}
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "DLX (Algorithm X): "
			<< duration.count() / 50.0 << " µs avg\n";
	}

	// Test bit-packed grid
	std::cout << "\n=== Bit-Packed Grid Test ===\n";

	auto packed = Sudoku::packGrid(puzzle);
	auto unpacked = Sudoku::unpackGrid(packed);
	bool match = true;
	for (int i = 0; i < 81; ++i)
		if (puzzle[i] != unpacked[i])
			match = false;

	std::cout << "Pack/Unpack: " << (match ? "PASS" : "FAIL") << "\n";
	std::cout << "Grid size: " << sizeof(Grid) << " bytes\n";
	std::cout << "PackedGrid size: " << sizeof(Sudoku::PackedGrid) << " bytes\n";

	return 0;
}