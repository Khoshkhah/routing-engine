cmake_minimum_required(VERSION 3.20)
project(routing_engine VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# H3 - try to find, or use local install
find_library(H3_LIBRARY NAMES h3 HINTS ${CMAKE_PREFIX_PATH}/lib $ENV{CONDA_PREFIX}/lib)
find_path(H3_INCLUDE_DIR NAMES h3/h3api.h HINTS ${CMAKE_PREFIX_PATH}/include $ENV{CONDA_PREFIX}/include)

if(NOT H3_LIBRARY OR NOT H3_INCLUDE_DIR)
    message(FATAL_ERROR "H3 library not found. Run: ./scripts/install_h3.sh")
endif()

message(STATUS "Found Arrow: ${Arrow_DIR}")
message(STATUS "Found Parquet: ${Parquet_DIR}")
message(STATUS "Found H3: ${H3_LIBRARY}")

# Create library
add_library(routing_lib STATIC
    src/shortcut_graph.cpp
    src/h3_utils.cpp
)

target_include_directories(routing_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${H3_INCLUDE_DIR}
)

target_link_libraries(routing_lib PUBLIC
    Arrow::arrow_shared
    Parquet::parquet_shared
    ${H3_LIBRARY}
)

# Create executable
add_executable(routing_engine src/main.cpp)
target_link_libraries(routing_engine PRIVATE routing_lib)

# Install
install(TARGETS routing_engine RUNTIME DESTINATION bin)
