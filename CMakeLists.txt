cmake_minimum_required(VERSION 3.16.3)
project(TarielBASIC)

include_directories(src)
aux_source_directory(src SRC)

# add_compile_options(-std=c11 -g)
add_executable(main ${SRC})