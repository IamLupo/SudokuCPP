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

void test_puzzle(const char* name, const Grid& puzzle)
{
	std::cout << "\n=== " << name << " ===\n";
	Grid sol1, sol2;

	Sudoku s1(puzzle);
	auto start1 = Clock::now();
	bool solved1 = s1.solve();
	auto end1 = Clock::now();

	// Get solution from Sudoku instance
	if (solved1) sol1 = s1.getSolution();

	SudokuDLX::solve(puzzle, sol2);
	auto start2 = Clock::now();
	bool solved2 = SudokuDLX::solve(puzzle, sol2);
	auto end2 = Clock::now();

	if (solved1 && solved2) {
		auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count();
		auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count();
		std::cout << "Bitmask: " << d1 << " µs\n";
		std::cout << "DLX: " << d2 << " µs\n";

		// Verify solutions match
		if (sol1 == sol2) {
			std::cout << "Solutions: MATCH\n";
		} else {
			std::cout << "Solutions: MISMATCH!\n";
			s1.Draw(false);
			SudokuDLX::Draw(sol2, false);
		}
	} else if (!solved1 && !solved2) {
		std::cout << "Both failed (unsolvable?)\n";
	} else {
		std::cout << "MISMATCH: Bitmask=" << (solved1 ? "solved" : "failed")
			<< " DLX=" << (solved2 ? "solved" : "failed") << "\n";
	}
}

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

	// Warmup - fair: both solvers get same warmup
	for (int i = 0; i < 5; ++i)
	{
		Sudoku s(puzzle);
		s.solve();
	}
	for (int i = 0; i < 5; ++i)
	{
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

	SudokuDLX::Draw(Sudoku::HARD_PUZZLE, true);
	std::cout << "\n";

	{
		Sudoku s(Sudoku::HARD_PUZZLE);
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
		bool solved = SudokuDLX::solve(Sudoku::HARD_PUZZLE, hard_solution);
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

	// Warmup - fair: both solvers get same warmup
	for (int i = 0; i < 3; ++i) {
		Sudoku s(Sudoku::HARD_PUZZLE);
		s.solve();
	}
	for (int i = 0; i < 3; ++i) {
		SudokuDLX::solve(Sudoku::HARD_PUZZLE, hard_solution);
	}

	// Benchmark bitmask solver on hard puzzle
	{
		auto start = Clock::now();
		for (int i = 0; i < 50; ++i)
		{
			Sudoku s(Sudoku::HARD_PUZZLE);
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
			SudokuDLX::solve(Sudoku::HARD_PUZZLE, hard_solution);
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

	// Correctness tests with multiple puzzles (skip deep backtrack as it's very slow)
	// test_puzzle("Deep Backtrack", Sudoku::DEEP_BACKTRACK);
	test_puzzle("17 Clues (Hard)", Sudoku::PUZZLE_17_CLUES);
	test_puzzle("Example Puzzle", Sudoku::EXAMPLE_PUZZLE);
	test_puzzle("AI Escargot", Sudoku::HARD_PUZZLE);

	return 0;
}