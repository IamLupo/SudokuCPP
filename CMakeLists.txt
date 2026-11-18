cmake_minimum_required(VERSION 3.10)
project(HelloWorld LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization options
option(ENABLE_LTO "Enable Link-Time Optimization" ON)
option(ENABLE_PGO "Enable Profile-Guided Optimization (PGO)" OFF)

# Compiler-specific optimization flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(BASE_OPT_FLAGS "-O3 -march=native")
    set(LTO_FLAGS "-flto=thin")
    set(PGO_GENERATE_FLAGS "-fprofile-generate")
    set(PGO_USE_FLAGS "-fprofile-use")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(BASE_OPT_FLAGS "-O3 -march=native")
    set(LTO_FLAGS "-flto")
    set(PGO_GENERATE_FLAGS "-fprofile-generate")
    set(PGO_USE_FLAGS "-fprofile-use")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(BASE_OPT_FLAGS "/O2 /arch:AVX2")
    set(LTO_FLAGS "/GL")
    # MSVC PGO requires separate generation/use builds
    set(PGO_GENERATE_FLAGS "/GENPROFILE /GL")
    set(PGO_USE_FLAGS "/USEPROFILE /GL")
else()
    set(BASE_OPT_FLAGS "")
    set(LTO_FLAGS "")
    set(PGO_GENERATE_FLAGS "")
    set(PGO_USE_FLAGS "")
endif()

# PGO support - requires two-phase build (generate profile, then use profile)
# Phase 1: Build with -fprofile-generate, run workload, merge profiles
# Phase 2: Build with -fprofile-use=<merged.profdata>
if(ENABLE_PGO AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    # Default to generate phase; user switches to use phase manually
    if(DEFINED ENV{PGO_USE_PROFILE})
        set(PGO_FLAGS "${PGO_USE_FLAGS}=$ENV{PGO_USE_PROFILE}")
        message(STATUS "PGO: USE phase (profile: $ENV{PGO_USE_PROFILE})")
    else()
        set(PGO_FLAGS "${PGO_GENERATE_FLAGS}")
        message(STATUS "PGO: GENERATE phase (run benchmarks, then rebuild with PGO_USE_PROFILE=merged.profdata)")
    endif()
elseif(ENABLE_PGO AND CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC two-phase PGO: /GENPROFILE then /USEPROFILE (separate build dirs)
    message(WARNING "MSVC PGO: Use manual two-phase build. See cmake --help for /GENPROFILE and /USEPROFILE")
    set(PGO_FLAGS "")
else()
    set(PGO_FLAGS "")
endif()

# LTO support
if(ENABLE_LTO)
    set(LTO_ENABLED_FLAGS "${LTO_FLAGS}")
else()
    set(LTO_ENABLED_FLAGS "")
endif()

# Combine all flags (Release only - Debug gets no optimization flags)
set(COMMON_FLAGS "${BASE_OPT_FLAGS} ${PGO_FLAGS} ${LTO_ENABLED_FLAGS}")

# Apply optimization flags ONLY to Release/RelWithDebInfo configs
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${COMMON_FLAGS}")
# Debug gets default flags (no /O2, no LTO, no AVX2)

add_executable(hello src/main.cpp src/Sudoku.cpp src/SudokuDLX.cpp)
target_include_directories(hello PRIVATE src)

if(ENABLE_LTO)
    if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_link_options(hello PRIVATE /LTCG)
    else()
        target_link_options(hello PRIVATE ${LTO_FLAGS})
    endif()
endif()

# Print configuration summary
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Optimization flags: ${COMMON_FLAGS}")
if(ENABLE_LTO)
    message(STATUS "LTO: ENABLED (${LTO_FLAGS})")
endif()
if(ENABLE_PGO AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    message(STATUS "PGO: ENABLED")
endif()