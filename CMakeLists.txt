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

# PGO support (only for Clang/GCC which support single-directory PGO)
if(ENABLE_PGO AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
    set(PGO_FLAGS "${PGO_GENERATE_FLAGS} ${PGO_USE_FLAGS}")
else()
    set(PGO_FLAGS "")
    if(ENABLE_PGO)
        message(WARNING "PGO with single-directory build only supported on Clang/GCC. On MSVC, use manual two-phase build.")
    endif()
endif()

# LTO support
if(ENABLE_LTO)
    set(LTO_ENABLED_FLAGS "${LTO_FLAGS}")
else()
    set(LTO_ENABLED_FLAGS "")
endif()

# Combine all flags
set(COMMON_FLAGS "${BASE_OPT_FLAGS} ${PGO_FLAGS} ${LTO_ENABLED_FLAGS}")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE "${COMMON_FLAGS}")

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