#!/bin/bash
# Two-phase PGO build script for Clang/GCC
# Usage: ./scripts/pgo_build.sh [clang|gcc]

set -euo pipefail

COMPILER="${1:-clang}"
BUILD_DIR_GEN="build_pgo_gen"
BUILD_DIR_USE="build_pgo_use"
PROFILE_DIR="pgo_profiles"
MERGED_PROFILE="${PROFILE_DIR}/merged.profdata"

echo "=== PGO Two-Phase Build for ${COMPILER} ==="

# Phase 1: Build with profile generation
echo "Phase 1: Building with -fprofile-generate in ${BUILD_DIR_GEN}..."
cmake -B "${BUILD_DIR_GEN}" -S . \
    -DCMAKE_CXX_COMPILER="${COMPILER}++" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_PGO=ON \
    -DENABLE_LTO=ON

cmake --build "${BUILD_DIR_GEN}" --config Release -j$(nproc)

# Run representative workload to generate profiles
echo "Phase 1b: Running benchmarks to generate profile data..."
mkdir -p "${PROFILE_DIR}"
# LLVM_PROFILE_FILE specifies where to write .profraw files
LLVM_PROFILE_FILE="${PROFILE_DIR}/profile-%p.profraw" \
    "${BUILD_DIR_GEN}/hello" 2>&1 | tail -20

# Merge profiles
echo "Phase 1c: Merging profiles..."
if command -v llvm-profdata >/dev/null 2>&1; then
    llvm-profdata merge -o "${MERGED_PROFILE}" "${PROFILE_DIR}"/profile-*.profraw
elif command -v gcc-profdata >/dev/null 2>&1; then
    # GCC doesn't have standard profdata tool; use gcov-tool
    echo "GCC: using gcov-tool (if available)"
    # gcov-tool merge ... (complex, skipping for now)
else
    echo "Warning: No profdata merge tool found. Install llvm-profdata or use GCC's gcov."
    exit 1
fi

echo "Merged profile: ${MERGED_PROFILE}"

# Phase 2: Build with profile use
echo "Phase 2: Building with -fprofile-use in ${BUILD_DIR_USE}..."
PGO_USE_PROFILE="${MERGED_PROFILE}" \
cmake -B "${BUILD_DIR_USE}" -S . \
    -DCMAKE_CXX_COMPILER="${COMPILER}++" \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_PGO=ON \
    -DENABLE_LTO=ON

cmake --build "${BUILD_DIR_USE}" --config Release -j$(nproc)

# Test PGO build
echo "Phase 2b: Testing PGO-optimized build..."
"${BUILD_DIR_USE}/hello" 2>&1 | tail -30

echo ""
echo "=== PGO Build Complete ==="
echo "Profile data: ${MERGED_PROFILE}"
echo "PGO build:    ${BUILD_DIR_USE}/hello"
echo ""
echo "To benchmark: ./${BUILD_DIR_USE}/hello"