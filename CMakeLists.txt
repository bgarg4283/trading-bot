cmake_minimum_required(VERSION 3.16)
project(NiftyScalpBot VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

# ─── Compiler Flags ───────────────────────────────────────────────────────────
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
add_compile_options(-Wall -Wextra -Wpedantic)

# ─── Dependencies ─────────────────────────────────────────────────────────────
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# nlohmann/json (header-only — fetched via FetchContent)
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

# ─── Sources ──────────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/FyersAPI.cpp
    src/IndicatorsEngine.cpp
    src/OptionChainAnalyzer.cpp
    src/StrategyEngine.cpp
    src/RiskManager.cpp
)

# ─── Executable ───────────────────────────────────────────────────────────────
add_executable(nifty_scalp_bot ${SOURCES})

target_include_directories(nifty_scalp_bot PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

target_link_libraries(nifty_scalp_bot PRIVATE
    ${CURL_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
    uuid
)

# ─── Install ──────────────────────────────────────────────────────────────────
install(TARGETS nifty_scalp_bot DESTINATION bin)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "CURL libraries: ${CURL_LIBRARIES}")
