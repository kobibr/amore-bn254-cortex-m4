cmake_minimum_required(VERSION 3.22)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(TC arm-none-eabi-)
set(CMAKE_C_COMPILER   ${TC}gcc)
set(CMAKE_CXX_COMPILER ${TC}g++)
set(CMAKE_ASM_COMPILER ${TC}gcc)
set(CMAKE_AR           ${TC}ar)
set(CMAKE_OBJCOPY      ${TC}objcopy)
set(CMAKE_SIZE         ${TC}size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(amorebn128 C ASM)
set(CMAKE_C_STANDARD 11)

# =============================================================================
#  Curve selection — BN254 or BLS12_381
# =============================================================================
set(CURVE "BLS12_381" CACHE STRING "Pairing curve: BN254 or BLS12_381")
set_property(CACHE CURVE PROPERTY STRINGS "BN254" "BLS12_381")

if(CURVE STREQUAL "BN254")
    set(CURVE_DEFINE "CURVE_BN254")
    set(CURVE_SUFFIX "bn254")
elseif(CURVE STREQUAL "BLS12_381")
    set(CURVE_DEFINE "CURVE_BLS12_381")
    set(CURVE_SUFFIX "bls12_381")
else()
    message(FATAL_ERROR "CURVE must be BN254 or BLS12_381 (got: ${CURVE})")
endif()

message(STATUS "Building AmorE for curve: ${CURVE}")

# Apply the curve definition to ALL targets via add_compile_definitions.
# (Cleaner than per-target target_compile_definitions when we have multiple
#  executables that all use the same curve.)
add_compile_definitions(${CURVE_DEFINE})

set(AMORE_TARGET "amore_${CURVE_SUFFIX}")


# =============================================================================
#  Paths — STM32 Cube and RELIC
# =============================================================================
if(NOT DEFINED CUBE_ROOT)
    set(CUBE_ROOT "${CMAKE_SOURCE_DIR}/STM32CubeF4")
endif()
set(HAL_PATH  "${CUBE_ROOT}/Drivers")
set(USB_PATH  "${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library")

set(RELIC_INSTALL "${CMAKE_SOURCE_DIR}/relic_install")

# =============================================================================
#  Source lists
# =============================================================================

# --- AmorE app sources (the original target) ---
set(AMORE_SRCS
    src/main.c
    src/fp.c
    src/fq.c
    src/fp2.c
    src/fp12.c
    src/g1.c
    src/g2.c
    src/amore.c
    src/amore_uart.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

# --- HAL sources (shared between targets) ---
set(HAL_SRCS
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_uart.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c
)

# --- RELIC benchmark app sources ---
set(RELIC_BENCH_SRCS
    src/pairing_main.c
    src/pairing_bench.c
    src/syscalls.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

# --- RELIC regression test sources (new) ---
set(REGRESSION_SRCS
    src/regression_main.c
    src/regression_test.c
    src/syscalls.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

# =============================================================================
#  Include paths
# =============================================================================
set(COMMON_INCLUDES
    inc
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Inc
    ${HAL_PATH}/CMSIS/Device/ST/STM32F4xx/Include
    ${HAL_PATH}/CMSIS/Include
    ${CMAKE_SOURCE_DIR}
)

# RELIC headers (only available after build-relic.sh is run)
set(RELIC_INCLUDES
    ${RELIC_INSTALL}/include
    ${RELIC_INSTALL}/include/relic
)

# =============================================================================
#  Compile flags
# =============================================================================
set(CPU_FLAGS  "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")
set(OPT_FLAGS  "-O2 -g3 -ffunction-sections -fdata-sections -fno-strict-aliasing")
set(WARN_FLAGS "-Wall -Wextra -Wno-unused-parameter")
set(DEF_FLAGS  "-DSTM32F407xx -DUSE_HAL_DRIVER")

set(CMAKE_C_FLAGS   "${CPU_FLAGS} ${OPT_FLAGS} ${WARN_FLAGS} ${DEF_FLAGS}" CACHE STRING "" FORCE)
set(CMAKE_ASM_FLAGS "${CPU_FLAGS} -x assembler-with-cpp" CACHE STRING "" FORCE)

set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/STM32F407VGTX_FLASH.ld")
set(CMAKE_EXE_LINKER_FLAGS
    "${CPU_FLAGS} -T${LINKER_SCRIPT} -Wl,--gc-sections -specs=nosys.specs -specs=nano.specs -lc -lm"
    CACHE STRING "" FORCE)

# =============================================================================
#  Target 1: AmorE (existing)
# =============================================================================
add_executable(${AMORE_TARGET}.elf ${AMORE_SRCS} ${HAL_SRCS})
target_include_directories(${AMORE_TARGET}.elf PRIVATE ${COMMON_INCLUDES})

add_custom_command(TARGET ${AMORE_TARGET}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${AMORE_TARGET}.elf> ${AMORE_TARGET}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${AMORE_TARGET}.elf> ${AMORE_TARGET}.bin
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:${AMORE_TARGET}.elf>
    COMMENT "AmorE: generating HEX and BIN"
)

# =============================================================================
#  Target 2: RELIC pairing benchmark
#
#  This target is only buildable AFTER scripts/build-relic.sh has been run.
#  If librelic_s.a doesn't exist yet, the target is skipped at configure time.
# =============================================================================
if(EXISTS "${RELIC_INSTALL}/lib/librelic_s.a")
    add_executable(relic_bench.elf ${RELIC_BENCH_SRCS} ${HAL_SRCS})
    target_include_directories(relic_bench.elf PRIVATE
        ${COMMON_INCLUDES}
        ${RELIC_INCLUDES}
    )
    target_link_libraries(relic_bench.elf PRIVATE
        ${RELIC_INSTALL}/lib/librelic_s.a
    )

    add_custom_command(TARGET relic_bench.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:relic_bench.elf> relic_bench.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:relic_bench.elf> relic_bench.bin
        COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:relic_bench.elf>
        COMMENT "RELIC bench: generating HEX and BIN"
    )

    message(STATUS "RELIC found at ${RELIC_INSTALL} — relic_bench.elf target enabled")

    # =========================================================================
    #  Target 3: Regression test for patched RELIC arm-asm-254 backend (new)
    #
    #  Validates fp_addn_low, fp_subn_low, fp_mulm_low, fp_sqrm_low, and
    #  fp_rdcn_low produce numerically correct outputs across constant
    #  vectors, algebraic identities, and limb-6 boundary cases.
    # =========================================================================
    add_executable(regression_test.elf ${REGRESSION_SRCS} ${HAL_SRCS})
    target_include_directories(regression_test.elf PRIVATE
        ${COMMON_INCLUDES}
        ${RELIC_INCLUDES}
    )
    target_link_libraries(regression_test.elf PRIVATE
        ${RELIC_INSTALL}/lib/librelic_s.a
    )

    add_custom_command(TARGET regression_test.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:regression_test.elf> regression_test.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:regression_test.elf> regression_test.bin
        COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:regression_test.elf>
        COMMENT "Regression test: generating HEX and BIN"
    )

    message(STATUS "regression_test.elf target enabled")
else()
    message(STATUS "RELIC not yet built — relic_bench.elf and regression_test.elf SKIPPED")
    message(STATUS "  Run scripts/build-relic.sh first, then re-run cmake")
endif()

# =============================================================================
#  Flash targets
# =============================================================================
find_program(ST_FLASH st-flash)
if(ST_FLASH)
    # Original AmorE flash target
    add_custom_target(flash
        COMMAND ${ST_FLASH} --reset write ${AMORE_TARGET}.bin 0x08000000
        DEPENDS ${AMORE_TARGET}.elf
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Flashing ${AMORE_TARGET}.bin to STM32..."
    )

    # Flash the RELIC bench
    if(TARGET relic_bench.elf)
        add_custom_target(flash_relic
            COMMAND ${ST_FLASH} --reset write relic_bench.bin 0x08000000
            DEPENDS relic_bench.elf
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Flashing relic_bench.bin to STM32..."
        )
    endif()

    # Flash the regression test
    if(TARGET regression_test.elf)
        add_custom_target(flash_regression
            COMMAND ${ST_FLASH} --reset write regression_test.bin 0x08000000
            DEPENDS regression_test.elf
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Flashing regression_test.bin to STM32..."
        )
    endif()
endif()
