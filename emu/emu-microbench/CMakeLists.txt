cmake_minimum_required (VERSION 3.3.1)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include (ExternalProject)

project(emu-microbench C CXX)

# Use the 2011 standards for C and C++
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
# Turn warnings into errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Werror")
# Enable Cilk
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcilkplus")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcilkplus")

# Disable some errant warnings caused by cilk_for
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused -Wno-uninitialized")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized")

# Arch-specific directives
if (CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    # Disable some errant warnings caused by cilk_for
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-cilk-loop-control-var-modification")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-cilk-loop-control-var-modification")
else()
    # Link with cilk runtime
    link_libraries(cilkrts)
endif()

# Link with emu_c_utils
find_package(emu_c_utils REQUIRED)
link_libraries(${EMU_C_UTILS_LIBRARIES})
include_directories(${EMU_C_UTILS_INCLUDE_DIRS})

set(ENABLE_VALIDATION "ON"
    CACHE BOOL "Validate results after each benchmark. The initialization and validation steps may spawn more
                threads than were requested on the command line."
)
if (NOT ENABLE_VALIDATION)
    add_definitions("-DNO_VALIDATE")
endif()

function(add_exe filename)
    string(REGEX REPLACE "\\.[^.]*$" "" name ${filename})
    add_executable(${name} ${filename})
    install(TARGETS ${name} RUNTIME DESTINATION ".")
endfunction()

add_exe(local_stream.c)
add_exe(global_stream.c)
add_exe(global_stream_1d.c)
add_exe(global_reduce.c)
add_exe(pointer_chase.c)
add_exe(ping_pong.c)
add_exe(local_sort.c)
add_exe(bulk_copy.c)
add_exe(scatter.c)
add_exe(malloc_free.c)
add_exe(spawn_rate.c)
add_exe(hot_range.c)
add_exe(hot_range_chunked.c)

set(ENABLE_CXX_BENCHMARKS OFF
    CACHE BOOL "Build C++ benchmarks, which rely on emu_cxx_utils.")
if (ENABLE_CXX_BENCHMARKS)
    # C++ benchmarks
    add_exe(local_stream_cxx.cc)
    add_exe(global_stream_cxx.cc)
endif()