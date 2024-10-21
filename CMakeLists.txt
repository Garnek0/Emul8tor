cmake_minimum_required(VERSION "3.13")

project("emul8tor")

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

file(GLOB_RECURSE SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable("emul8tor" "${SOURCE_FILES}")

target_include_directories("emul8tor" PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include/")
target_include_directories("emul8tor" PUBLIC ${SDL2_INCLUDE_DIRS})

target_link_libraries("emul8tor" ${SDL2_LIBRARIES})

