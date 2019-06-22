cmake_minimum_required(VERSION 3.13)
project(stgen_1to2)

set(CMAKE_CXX_STANDARD 17)
add_executable(stgen_1to2 src/main.cpp)

target_sources(stgen_1to2 PRIVATE
    third_party/zlib/contrib/iostream3/zfstream.cc
)

target_include_directories(stgen_1to2 PRIVATE
    third_party/zlib/contrib/iostream3
)

add_subdirectory(third_party/fmt)
target_link_libraries(stgen_1to2 z pthread stdc++fs fmt::fmt)
