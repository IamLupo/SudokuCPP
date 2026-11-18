#include <Sudoku.hpp>
#include <iostream>
#include <algorithm>

#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(__popcnt)
#pragma intrinsic(_tzcnt_u32)
#endif

// AVX2 support detection (disabled in Debug builds due to stack alignment issues)
#if (defined(__AVX2__) || defined(_M_AVX2)) && !defined(_DEBUG)
#define SUDOKU_AVX2 1
#include <immintrin.h>
#else
#define SUDOKU_AVX2 0
#endif

using Mask = Sudoku::Mask;


// Portable popcount
inline int popcount(Mask m)
{
#if defined(_MSC_VER)
    return __popcnt(static_cast<unsigned int>(m));
#else
    return __builtin_popcount(static_cast<unsigned int>(m));
#endif
}

// Portable count trailing zeros (input must be non-zero)
inline int ctz(Mask m)
{
#if defined(_MSC_VER)
    unsigned long index;
    _BitScanForward(&index, static_cast<unsigned long>(m));
    return static_cast<int>(index);
#else
    return __builtin_ctz(static_cast<unsigned int>(m));
#endif
}

#if SUDOKU_AVX2
// AVX2-optimized namespace for vectorized operations
namespace SudokuAVX2
{
    // 16 x 16-bit lanes in a 256-bit register
    static constexpr int LANES = 16;
    static constexpr Mask ALL_DIGITS = 0x3FE; // bits 1-9 set

    // Broadcast a 16-bit mask to all 16 lanes
    inline __m256i broadcast_mask(Mask m)
    {
        return _mm256_set1_epi16(static_cast<short>(m));
    }

    // Load 16 cell candidates (unaligned, 16 * 2 = 32 bytes)
    inline __m256i load_candidates(const Mask* ptr)
    {
        return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
    }

    // Store 16 cell candidates
    inline void store_candidates(Mask* ptr, __m256i val)
    {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), val);
    }

    // Vectorized popcount for 16 x 16-bit values using nibble lookup table
    // Returns popcount for each 16-bit lane in lower 8 bits of each 16-bit lane
    inline __m256i popcount_epi16(__m256i x)
    {
        // Split each 16-bit lane into 4 nibbles (4 bits each)
        // Use 16-entry lookup table (popcount for 0-15) duplicated across 256-bit lanes
        const __m256i popcnt_lut = _mm256_setr_epi8(
            0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,  // lane 0: popcount for 0-15
            0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4   // lane 1: same (shuffle operates per 128-bit lane)
        );

        // Extract low nibbles of each byte
        __m256i nibble_mask = _mm256_set1_epi8(0x0F);
        __m256i lo_nibble = _mm256_and_si256(x, nibble_mask);
        __m256i hi_nibble = _mm256_and_si256(_mm256_srli_epi16(x, 4), nibble_mask);
        __m256i lo2_nibble = _mm256_and_si256(_mm256_srli_epi16(x, 8), nibble_mask);
        __m256i hi2_nibble = _mm256_and_si256(_mm256_srli_epi16(x, 12), nibble_mask);

        // Lookup popcount for each nibble
        __m256i cnt0 = _mm256_shuffle_epi8(popcnt_lut, lo_nibble);
        __m256i cnt1 = _mm256_shuffle_epi8(popcnt_lut, hi_nibble);
        __m256i cnt2 = _mm256_shuffle_epi8(popcnt_lut, lo2_nibble);
        __m256i cnt3 = _mm256_shuffle_epi8(popcnt_lut, hi2_nibble);

        // Sum all 4 nibbles per 16-bit lane
        __m256i sum = _mm256_add_epi8(cnt0, cnt1);
        sum = _mm256_add_epi8(sum, cnt2);
        sum = _mm256_add_epi8(sum, cnt3);

        // Mask to keep only low byte of each 16-bit lane (popcount fits in 4 bits max=9)
        return _mm256_and_si256(sum, _mm256_set1_epi16(0x00FF));
    }

    // Check which lanes have exactly 1 candidate (naked singles)
    // Returns mask where each 16-bit lane is 0xFFFF if exactly 1 bit set, 0 otherwise
    inline __m256i naked_singles_mask(__m256i candidates)
    {
        // (candidates & (candidates - 1)) == 0 && candidates != 0
        __m256i dec = _mm256_sub_epi16(candidates, _mm256_set1_epi16(1));
        __m256i and_result = _mm256_and_si256(candidates, dec);
        __m256i is_zero = _mm256_cmpeq_epi16(and_result, _mm256_setzero_si256());
        __m256i non_zero = _mm256_cmpgt_epi16(candidates, _mm256_setzero_si256());
        return _mm256_and_si256(is_zero, non_zero);
    }

    // Clear a specific digit mask from all candidates (vectorized)
    // candidates &= ~mask
    inline __m256i clear_digit(__m256i candidates, Mask digit_mask)
    {
        __m256i mask = broadcast_mask(~digit_mask);
        return _mm256_and_si256(candidates, mask);
    }

    // Compute candidates for 16 cells with same row/col/box (rare case)
    // More commonly: compute for cells in same row (different cols, same row mask)
    inline __m256i compute_candidates_row(__m256i row_mask, __m256i col_masks, __m256i box_masks)
    {
        __m256i combined = _mm256_or_si256(_mm256_or_si256(row_mask, col_masks), box_masks);
        __m256i all_digits = broadcast_mask(ALL_DIGITS);
        return _mm256_andnot_si256(combined, all_digits);
    }

    // Find index of lane with minimum popcount (for MRV heuristic)
    // Returns -1 if all lanes are filled (candidates == 0)
    inline int find_min_candidates(const Mask* candidates, int count, int& min_count)
    {
        min_count = 10;
        int best_idx = -1;

        // Process 16 at a time
        int i = 0;
        for (; i + LANES <= count; i += LANES) {
            __m256i vec = load_candidates(&candidates[i]);
            __m256i popcnts = popcount_epi16(vec);

            // Extract popcounts and find minimum among non-zero candidates
            alignas(32) unsigned char counts[LANES];
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(counts), popcnts);

            for (int j = 0; j < LANES; ++j) {
                int c = counts[j];
                if (c > 0 && c < min_count) {
                    min_count = c;
                    best_idx = i + j;
                    if (c <= 1) return best_idx;
                }
            }
        }

        // Scalar tail
        for (; i < count; ++i) {
            if (candidates[i] != 0) {
                int c = popcount(candidates[i]);
                if (c < min_count) {
                    min_count = c;
                    best_idx = i;
                    if (c <= 1) return best_idx;
                }
            }
        }

        return best_idx;
    }
}
#endif // SUDOKU_AVX2

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

	// Initialize masks from givens with validation
	for (int r = 0; r < N; ++r) {
		for (int c = 0; c < N; ++c) {
			int num = _solution[idx(r, c)];
			if (num != 0) {
				// Validate input range
				if (num < 1 || num > 9) return false;
				Mask m = DIGIT_MASK[num];
				// Check for conflicting givens (same digit already in row/col/box)
				if ((_rows[r] & m) || (_cols[c] & m) || (_boxes[BOX_OF[idx(r, c)]] & m))
					return false;
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
	return popcount(m);
}

int Sudoku::_findBestCell() const
{
#if SUDOKU_AVX2
	// Vectorized: find cell with minimum candidates among empty cells
	// Direct load from member arrays (no gather/scatter overhead)
	int min_count = 10;
	int best_idx = -1;

	// Process 16 cells at a time
	for (int base = 0; base < 81; base += 16) {
		int remaining = 81 - base;
		int process = (remaining >= 16) ? 16 : remaining;

		// Direct load from member arrays (unaligned is fine for AVX2)
		__m256i vec_cand = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&_cell_candidates[base]));
		__m256i vec_sol = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&_solution[base]));

		// Mask out filled cells (set candidates to 0 where solution != 0)
		__m256i sol_zero = _mm256_cmpeq_epi16(vec_sol, _mm256_setzero_si256());
		vec_cand = _mm256_and_si256(vec_cand, sol_zero);

		// For partial final chunk, zero out of bounds lanes
		if (process < 16) {
			__m256i mask = _mm256_setr_epi16(
				(process > 0) ? -1 : 0, (process > 1) ? -1 : 0, (process > 2) ? -1 : 0, (process > 3) ? -1 : 0,
				(process > 4) ? -1 : 0, (process > 5) ? -1 : 0, (process > 6) ? -1 : 0, (process > 7) ? -1 : 0,
				(process > 8) ? -1 : 0, (process > 9) ? -1 : 0, (process > 10) ? -1 : 0, (process > 11) ? -1 : 0,
				(process > 12) ? -1 : 0, (process > 13) ? -1 : 0, (process > 14) ? -1 : 0, (process > 15) ? -1 : 0
			);
			vec_cand = _mm256_and_si256(vec_cand, mask);
		}

		// Compute popcounts (each 16-bit lane has popcount in low 8 bits)
		__m256i popcnts = SudokuAVX2::popcount_epi16(vec_cand);

		// Extract low bytes (popcount per lane) - 16 bytes total
		alignas(32) unsigned char counts[16];
		__m256i popcnts_bytes = _mm256_shuffle_epi8(
			popcnts,
			_mm256_setr_epi8(
				0, 2, 4, 6, 8, 10, 12, 14,  // even indices = low bytes of lanes 0-7
				16, 18, 20, 22, 24, 26, 28, 30, // even indices = low bytes of lanes 8-15
				-1,-1,-1,-1,-1,-1,-1,-1,
				-1,-1,-1,-1,-1,-1,-1,-1
			)
		);
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(counts), popcnts_bytes);

		for (int j = 0; j < process; ++j) {
			int c = counts[j];
			// Only flag contradiction for EMPTY cells (solution == 0)
			// Filled cells have c == 0 after masking but are not contradictions
			if (c == 0 && _solution[base + j] == 0) {
				// Contradiction: empty cell with no candidates -> fail fast
				return base + j;
			}
			if (c > 0 && c < min_count) {
				min_count = c;
				best_idx = base + j;
				if (c <= 1) return best_idx;
			}
		}
	}

	return best_idx;
#else
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
#endif
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

#if SUDOKU_AVX2
	// Vectorized: find all naked singles in chunks of 16
	// Direct load from member arrays (no gather/scatter overhead)
	for (int base = 0; base < 81; base += 16) {
		int remaining = 81 - base;
		int process = (remaining >= 16) ? 16 : remaining;

		// Direct load from member arrays (unaligned is fine for AVX2)
		__m256i vec_cand = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&_cell_candidates[base]));
		__m256i vec_sol = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&_solution[base]));

		// Mask out filled cells
		__m256i sol_zero = _mm256_cmpeq_epi16(vec_sol, _mm256_setzero_si256());
		vec_cand = _mm256_and_si256(vec_cand, sol_zero);

		// For partial final chunk, zero out of bounds lanes
		if (process < 16) {
			__m256i mask = _mm256_setr_epi16(
				(process > 0) ? -1 : 0, (process > 1) ? -1 : 0, (process > 2) ? -1 : 0, (process > 3) ? -1 : 0,
				(process > 4) ? -1 : 0, (process > 5) ? -1 : 0, (process > 6) ? -1 : 0, (process > 7) ? -1 : 0,
				(process > 8) ? -1 : 0, (process > 9) ? -1 : 0, (process > 10) ? -1 : 0, (process > 11) ? -1 : 0,
				(process > 12) ? -1 : 0, (process > 13) ? -1 : 0, (process > 14) ? -1 : 0, (process > 15) ? -1 : 0
			);
			vec_cand = _mm256_and_si256(vec_cand, mask);
		}

		// Find naked singles: exactly 1 bit set
		__m256i singles_mask = SudokuAVX2::naked_singles_mask(vec_cand);

		// Extract which lanes are naked singles (16-bit mask per lane: 0xFFFF or 0)
		alignas(32) uint16_t mask_arr[16];
		_mm256_storeu_si256(reinterpret_cast<__m256i*>(mask_arr), singles_mask);

		for (int j = 0; j < process; ++j) {
			if (mask_arr[j]) {
				int i = base + j;
				Mask candidates = _cell_candidates[i];
				int num = ctz(candidates);
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
#else
	for (int i = 0; i < 81; ++i) {
		if (_solution[i] == 0) {
			Mask candidates = _cell_candidates[i];
			if (candidates && (candidates & (candidates - 1)) == 0) {
				int num = ctz(candidates);
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
#endif

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

	// Check boxes - use precomputed BOX_CELLS for 9x speedup
	for (int b = 0; b < 9; ++b) {
		for (int d = 1; d <= 9; ++d) {
			Mask m = DIGIT_MASK[d];
			if (_boxes[b] & m) continue;
			int count = 0, last_cell = -1;
			// Iterate only 9 cells in this box instead of all 81
			for (int k = 0; k < 9; ++k) {
				int i = BOX_CELLS[b][k];
				if (_solution[i] == 0 && (_cell_candidates[i] & m)) {
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
	if (cell == -1) {
		// No cell with candidates found - check if solved or contradiction
		for (int i = 0; i < 81; ++i) {
			if (_solution[i] == 0) return false; // empty cell with 0 candidates = contradiction
		}
		return true; // completely filled = solved
	}

	int row = cell / 9;
	int col = cell % 9;
	Mask candidates = _cell_candidates[cell];
	int b = BOX_OF[cell];

	// Try each candidate
	while (candidates) {
		Mask m = candidates & -candidates;
		int num = ctz(m);
		candidates &= candidates - 1;

		// Place digit
		_solution[cell] = num;
		_rows[row] |= m;
		_cols[col] |= m;
		_boxes[b] |= m;

		// Save and update affected candidates (only peers: same row, col, or box)
		// Use PEERS lookup - only 20 peers per cell
		Mask saved[20];
		for (int k = 0; k < 20; ++k) {
			int peer = PEERS[cell][k];
			saved[k] = _cell_candidates[peer];
			if (_solution[peer] == 0) {
				_cell_candidates[peer] &= ~m;
			}
		}

		// Apply constraint propagation after placement
		if (_propagate()) {
			if (_solveRecursive())
				return true;
		}

		// Backtrack
		_solution[cell] = 0;
		_rows[row] &= ~m;
		_cols[col] &= ~m;
		_boxes[b] &= ~m;

		// Restore candidates for peers
		for (int k = 0; k < 20; ++k) {
			int peer = PEERS[cell][k];
			_cell_candidates[peer] = saved[k];
		}
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