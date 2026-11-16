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
	if (solved) {
		std::cout << "SOLVED in " << duration.count() << " µs\n";
	} else {
		std::cout << "FAILED\n";
	}
}

int main()
{
	std::cout << "Starting...\n";
	Grid puzzle = Sudoku::EXAMPLE_PUZZLE;
	Grid solution;

	std::cout << "=== Sudoku Solver Benchmark ===\n\n";

	std::cout << "START SUDOKU:\n";
	Sudoku sudoku(puzzle);
	sudoku.Draw(true);
	std::cout << "\n";

	// Test bitmask + MRV + propagation solver
	{
		Grid sol;
		Sudoku s(puzzle);
		s.solve();
		sol = s.getSolution();
		std::cout << "Bitmask + MRV + Propagation:\n";
		s.Draw(false);
	}

	std::cout << "\n---\n";

	// Test DLX solver
	{
		Grid sol;
		benchmark<SudokuDLX>("DLX (Algorithm X)", puzzle, sol);
		if (SudokuDLX::validate(sol)) {
			std::cout << "DLX Solution:\n";
			for (int r = 0; r < 9; ++r) {
				for (int c = 0; c < 9; ++c) {
					std::cout << sol[r][c] << ' ';
				}
				std::cout << '\n';
			}
		}
	}

	std::cout << "\n---\n";

	// Benchmark multiple runs (use example puzzle which is easier)
	std::cout << "\n=== Performance Test (100 iterations on example puzzle) ===\n";

	Grid sol;

	// Warmup
	for (int i = 0; i < 5; ++i) {
		Sudoku s(puzzle);
		s.solve();
		SudokuDLX::solve(puzzle, sol);
	}

	// Benchmark bitmask solver
	{
		auto start = Clock::now();
		for (int i = 0; i < 100; ++i) {
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
		for (int i = 0; i < 100; ++i) {
			SudokuDLX::solve(puzzle, sol);
		}
		auto end = Clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
		std::cout << "DLX (Algorithm X): "
			<< duration.count() / 100.0 << " µs avg\n";
	}

	// Test bit-packed grid
	std::cout << "\n=== Bit-Packed Grid Test ===\n";
	auto packed = Sudoku::packGrid(puzzle);
	auto unpacked = Sudoku::unpackGrid(packed);
	bool match = true;
	for (int r = 0; r < 9; ++r) {
		for (int c = 0; c < 9; ++c) {
			if (puzzle[r][c] != unpacked[r][c]) match = false;
		}
	}
	std::cout << "Pack/Unpack: " << (match ? "PASS" : "FAIL") << "\n";
	std::cout << "Packed size: " << sizeof(Sudoku::PackedGrid) << " bytes (vs " << sizeof(Grid) << ")\n";

	return 0;
}