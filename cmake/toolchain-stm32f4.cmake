# =============================================================================
#  toolchain-stm32f4.cmake — CMake toolchain for STM32F407 (Cortex-M4 + FPU)
#  Used both for building RELIC as a static library AND for our app.
# =============================================================================
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(TC arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TC}gcc)
set(CMAKE_CXX_COMPILER ${TC}g++)
set(CMAKE_ASM_COMPILER ${TC}gcc)
set(CMAKE_AR           ${TC}ar)
set(CMAKE_OBJCOPY      ${TC}objcopy)
set(CMAKE_SIZE         ${TC}size)

# Don't try to run target binaries during configure
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Common flags for all targets compiled with this toolchain
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS} -x assembler-with-cpp")

# Tell RELIC's CMake we're on ARM 32-bit
set(ARCH "ARM" CACHE STRING "" FORCE)
set(WSIZE 32   CACHE STRING "" FORCE)
