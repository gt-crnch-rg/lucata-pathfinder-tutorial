# - Try to find emu_c_utils
#  Once done this will define
#  EMU_C_UTILS_FOUND - System has emu_c_utils
#  EMU_C_UTILS_INCLUDE_DIRS - The emu_c_utils include directories
#  EMU_C_UTILS_LIBRARIES - The libraries needed to use emu_c_utils
#  EMU_C_UTILS_DEFINITIONS - Compiler switches required for using emu_c_utils

# Find Emu install dir
if (LLVM_CILK)
    set(EMU_PREFIX ${LLVM_CILK})
else()
    set(EMU_PREFIX "/tools/emu/emu-19.02")
endif()

# Append platform-specific suffix
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emu1")
    set(EMU_PREFIX ${EMU_PREFIX}/x86)
endif()

find_path(EMU_C_UTILS_INCLUDE_DIR
    NAMES emu_c_utils/emu_c_utils.h
    HINTS ${EMU_PREFIX}/include
)
find_library(EMU_C_UTILS_LIBRARY
    NAMES emu_c_utils
    HINTS ${EMU_PREFIX}/lib
)
include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set emu_c_utils_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(emu_c_utils
    DEFAULT_MSG
    EMU_C_UTILS_LIBRARY
    EMU_C_UTILS_INCLUDE_DIR
)

mark_as_advanced(EMU_C_UTILS_INCLUDE_DIR EMU_C_UTILS_LIBRARY )
set(EMU_C_UTILS_LIBRARIES ${EMU_C_UTILS_LIBRARY} )
set(EMU_C_UTILS_INCLUDE_DIRS ${EMU_C_UTILS_INCLUDE_DIR} )
