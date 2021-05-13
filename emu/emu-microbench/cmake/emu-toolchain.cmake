# Pass this file to the first invocation of cmake using -DCMAKE_TOOLCHAIN_FILE=
set(CMAKE_SYSTEM_NAME Emu1)

if (LLVM_CILK)
    set(LLVM_CILK_HOME ${LLVM_CILK})
else()
    set(LLVM_CILK_HOME "/tools/emu/emu-19.02")
endif()
message(STATUS "Using Emu1 toolchain in ${LLVM_CILK_HOME}")

set(CMAKE_C_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_CXX_COMPILER "${LLVM_CILK_HOME}/bin/emu-cc.sh")
set(CMAKE_FIND_ROOT_PATH ${LLVM_CILK_HOME}/gossamer64)
set(EMU_SIMULATOR "${LLVM_CILK_HOME}/bin/emusim.x")
