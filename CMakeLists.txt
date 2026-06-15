cmake_minimum_required(VERSION 3.16)
project(NiftyScalpBot VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")

# ── Compiler flags ─────────────────────────────────────────────────────────────
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
# Suppress unused-parameter warnings (fixed in code with /**/ comments)
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

# ── Find CURL ──────────────────────────────────────────────────────────────────
find_package(CURL QUIET)

if(NOT CURL_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED libcurl)
endif()

if(NOT CURL_FOUND)
    # Manual fallback
    find_library(CURL_LIBRARIES NAMES curl libcurl
        PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
    find_path(CURL_INCLUDE_DIRS curl/curl.h
        PATHS /usr/include /usr/local/include /usr/include/x86_64-linux-gnu)
endif()

message(STATUS "CURL libs:    ${CURL_LIBRARIES}")
message(STATUS "CURL include: ${CURL_INCLUDE_DIRS}")

# ── Threads ────────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── nlohmann/json (header-only) ───────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(nlohmann_json)

# ── Sources ────────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/FyersAPI.cpp
    src/IndicatorsEngine.cpp
    src/OptionChainAnalyzer.cpp
    src/StrategyEngine.cpp
    src/RiskManager.cpp
)

# ── Executable ─────────────────────────────────────────────────────────────────
add_executable(nifty_scalp_bot ${SOURCES})

target_include_directories(nifty_scalp_bot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(nifty_scalp_bot PRIVATE
    ${CURL_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# ── Install ────────────────────────────────────────────────────────────────────
install(TARGETS nifty_scalp_bot DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CXX standard: ${CMAKE_CXX_STANDARD}")
