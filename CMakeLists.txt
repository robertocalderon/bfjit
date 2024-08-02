
cmake_minimum_required(VERSION 3.16)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(
    BFJIT
    LANGUAGES CXX
    VERSION 0.1.0
)

include(FetchContent)
FetchContent_Declare(
	fmtlib
	GIT_REPOSITORY 	https://www.github.com/fmtlib/fmt
	GIT_TAG		8.1.1
)
FetchContent_Declare(
	asmjit
	GIT_REPOSITORY 	https://www.github.com/asmjit/asmjit
	GIT_TAG		052b4430e93f021709d27d2b5d42694ce0c49ce6
)
FetchContent_MakeAvailable(fmtlib asmjit)

add_executable(bfjit)
target_compile_features(bfjit PUBLIC cxx_std_20)
target_link_libraries(bfjit asmjit::asmjit fmt::fmt)
target_sources(bfjit PRIVATE
    "src/main.cpp"
    "src/parser.cpp"
    "src/interpreter.cpp"
    "src/optimizer.cpp"
    "src/jit.cpp"
)
