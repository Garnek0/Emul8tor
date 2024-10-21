cmake_minimum_required(VERSION "3.13")

project("emul8tor")

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED)

add_executable("emul8tor" "chip8.c")

target_include_directories("emul8tor" PUBLIC ${SDL2_INCLUDE_DIRS})

target_link_libraries("emul8tor" ${SDL2_LIBRARIES})

