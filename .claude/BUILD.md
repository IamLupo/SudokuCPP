## Build Commands

### Release
```bash  
cmake -B build -S .
cmake --build build --config Release

# Run
./build/Release/hello.exe
```

### Debug
```bash  
cmake -B build -S .
cmake --build build --config Debug

# Run
./build/Debug/hello.exe
```

### Profile-Guided Optimization (PGO) — Two-Phase Build

PGO requires two separate builds: **generate** (instrumented) → run workload → **use** (optimized).

**Clang/GCC (Linux/macOS/WSL):**
```bash
# Phase 1: Generate profile data
cmake -B build_pgo_gen -S . -DENABLE_PGO=ON -DENABLE_LTO=ON
cmake --build build_pgo_gen --config Release

# Run workload to generate .profraw files
LLVM_PROFILE_FILE="pgo_profiles/profile-%p.profraw" ./build_pgo_gen/hello

# Merge profiles
llvm-profdata merge -o pgo_profiles/merged.profdata pgo_profiles/profile-*.profraw

# Phase 2: Build using profile data
PGO_USE_PROFILE=pgo_profiles/merged.profdata cmake -B build_pgo_use -S . -DENABLE_PGO=ON -DENABLE_LTO=ON
cmake --build build_pgo_use --config Release

# Run PGO-optimized binary
./build_pgo_use/hello
```

**Or use the helper script:**
```bash
./scripts/pgo_build.sh clang   # or gcc
```

**MSVC (Windows):**
```bash
# Phase 1: Generate (separate build dir)
cmake -B build_pgo_gen -S . -DENABLE_PGO=ON -DENABLE_LTO=ON
cmake --build build_pgo_gen --config Release

# Run to generate .pgc/.pgd files
./build_pgo_gen/Release/hello.exe

# Phase 2: Use (separate build dir, different flags)
# Note: MSVC PGO requires manual /GENPROFILE /USEPROFILE on command line
# See: https://learn.microsoft.com/en-us/cpp/build/reference/profile-guided-optimizations
```