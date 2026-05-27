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
    set(FP_LIMBS_VAL 8)
elseif(CURVE STREQUAL "BLS12_381")
    set(CURVE_DEFINE "CURVE_BLS12_381")
    set(CURVE_SUFFIX "bls12_381")
    set(FP_LIMBS_VAL 12)
else()
    message(FATAL_ERROR "CURVE must be BN254 or BLS12_381 (got: ${CURVE})")
endif()

message(STATUS "Building AmorE for curve: ${CURVE}")

# Apply the curve definition to ALL targets via add_compile_definitions.
# (Cleaner than per-target target_compile_definitions when we have multiple
#  executables that all use the same curve.)
add_compile_definitions(${CURVE_DEFINE} FP_LIMBS=${FP_LIMBS_VAL})

set(AMORE_TARGET "amore_${CURVE_SUFFIX}")

# =============================================================================
#  Measurement mode + benchmark size (Pre-PPK2 phase)
# =============================================================================
set(MEASUREMENT_MODE "A" CACHE STRING
    "Energy measurement mode: A=AmorE protocol, B=direct N-pairing loop")
set_property(CACHE MEASUREMENT_MODE PROPERTY STRINGS "A" "B")

set(BENCH_N "10" CACHE STRING
    "Iteration count for Mode B (number of pp_map_oatep_k12 calls)")

set(AMORE_TRIGGERS_ENABLED "1" CACHE STRING
    "1 = emit GPIO triggers around measurable phases, 0 = no triggers")

add_compile_definitions(AMORE_TRIGGERS_ENABLED=${AMORE_TRIGGERS_ENABLED})

if(MEASUREMENT_MODE STREQUAL "B")
    add_compile_definitions(PB_N_ITER=${BENCH_N}u)
    message(STATUS "Mode B selected: BENCH_N=${BENCH_N} pairings per run")
else()
    message(STATUS "Mode A selected (AmorE protocol)")
endif()

message(STATUS "GPIO triggers: ${AMORE_TRIGGERS_ENABLED}")


# =============================================================================
#  Paths — STM32 Cube and RELIC
# =============================================================================
if(NOT DEFINED CUBE_ROOT)
    set(CUBE_ROOT "${CMAKE_SOURCE_DIR}/STM32CubeF4")
endif()
set(HAL_PATH  "${CUBE_ROOT}/Drivers")
set(USB_PATH  "${CUBE_ROOT}/Middlewares/ST/STM32_USB_Device_Library")

# ============================================================================
#  RELIC install path — curve-aware (apples-to-apples C-only builds)
# ============================================================================
# For fair comparison vs AmorE (pure C, no asm), both curves use RELIC with
# ARITH=easy (generic C, no hand-tuned assembly).
#
#   BN254     → relic_install_bn254_easy   (built by build-relic-bn254-easy.sh)
#   BLS12_381 → relic_install_bls381       (built by build-relic-bls381.sh)
#
# The arm-asm-254 build (in relic_install/) is preserved as a reference for
# showing what hand-tuned assembly achieves, but is NOT used in the energy
# campaign — mixing C and asm would not be apples-to-apples.
if(CURVE STREQUAL "BN254")
    set(RELIC_INSTALL "${CMAKE_SOURCE_DIR}/relic_install_bn254_easy")
elseif(CURVE STREQUAL "BLS12_381")
    set(RELIC_INSTALL "${CMAKE_SOURCE_DIR}/relic_install_bls381")
else()
    message(FATAL_ERROR "Unknown CURVE for RELIC selection: ${CURVE}")
endif()

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
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc.c
    ${HAL_PATH}/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rtc_ex.c
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
set(OPT_FLAGS  "-O3 -g3 -ffunction-sections -fdata-sections -fno-strict-aliasing")
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
    # Curve-aware target name: relic_bench_${CURVE_SUFFIX}.elf to match
    # AmorE convention (amore_bn254.elf, amore_bls12_381.elf).
    set(RELIC_BENCH_TARGET "relic_bench_${CURVE_SUFFIX}")
    add_executable(${RELIC_BENCH_TARGET}.elf ${RELIC_BENCH_SRCS} ${HAL_SRCS})
    target_include_directories(${RELIC_BENCH_TARGET}.elf PRIVATE
        ${COMMON_INCLUDES}
        ${RELIC_INCLUDES}
    )
    target_link_libraries(${RELIC_BENCH_TARGET}.elf PRIVATE
        ${RELIC_INSTALL}/lib/librelic_s.a
    )

    add_custom_command(TARGET ${RELIC_BENCH_TARGET}.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:${RELIC_BENCH_TARGET}.elf> ${RELIC_BENCH_TARGET}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${RELIC_BENCH_TARGET}.elf> ${RELIC_BENCH_TARGET}.bin
        COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:${RELIC_BENCH_TARGET}.elf>
        COMMENT "RELIC bench: generating HEX and BIN"
    )

    message(STATUS "RELIC found at ${RELIC_INSTALL} — ${RELIC_BENCH_TARGET}.elf target enabled")

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
#  Target 4: Stop-mode test (Pre-PPK2)
#
#  Enters Stop mode, wakes every 5s for ~30s total (6 cycles). Toggles PA0
#  on each transition and uses LED to indicate state visually.
#  No UART, no RELIC, no AmorE protocol.
# =============================================================================
set(STOP_TEST_SRCS
    src/stop_main.c
    src/lowpower.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

add_executable(stop_test.elf ${STOP_TEST_SRCS} ${HAL_SRCS})
target_include_directories(stop_test.elf PRIVATE ${COMMON_INCLUDES})

add_custom_command(TARGET stop_test.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:stop_test.elf> stop_test.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:stop_test.elf> stop_test.bin
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:stop_test.elf>
    COMMENT "stop_test: generating HEX and BIN"
)

# =============================================================================
#  Target 5: Wake-up burst (Pre-PPK2)
#
#  100 consecutive Stop→Run cycles with RTC wakeup ~10 ms each.
#  Toggles PA0 high during Run, low during Stop. Total runtime ~1s.
# =============================================================================
set(STOP_TEST_NOSTOP_SRCS
    src/stop_test_nostop_main.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

add_executable(stop_test_nostop.elf ${STOP_TEST_NOSTOP_SRCS} ${HAL_SRCS})
target_include_directories(stop_test_nostop.elf PRIVATE ${COMMON_INCLUDES})

add_custom_command(TARGET stop_test_nostop.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:stop_test_nostop.elf> stop_test_nostop.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:stop_test_nostop.elf> stop_test_nostop.bin
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:stop_test_nostop.elf>
    COMMENT "stop_test_nostop: generating HEX and BIN"
)

set(WAKEUP_BURST_SRCS
    src/wakeup_burst_main.c
    src/lowpower.c
    src/startup_stm32f407xx.c
    ${CUBE_ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c
)

add_executable(wakeup_burst.elf ${WAKEUP_BURST_SRCS} ${HAL_SRCS})
target_include_directories(wakeup_burst.elf PRIVATE ${COMMON_INCLUDES})

add_custom_command(TARGET wakeup_burst.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:wakeup_burst.elf> wakeup_burst.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:wakeup_burst.elf> wakeup_burst.bin
    COMMAND ${CMAKE_SIZE}              $<TARGET_FILE:wakeup_burst.elf>
    COMMENT "wakeup_burst: generating HEX and BIN"
)

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

    add_custom_target(flash_stop_test
        COMMAND ${ST_FLASH} --reset write stop_test.bin 0x08000000
        DEPENDS stop_test.elf
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Flashing stop_test.bin to STM32..."
    )

    add_custom_target(flash_wakeup_burst
        COMMAND ${ST_FLASH} --reset write wakeup_burst.bin 0x08000000
        DEPENDS wakeup_burst.elf
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Flashing wakeup_burst.bin to STM32..."
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

# ─────────────────────────────────────────────────────────────────────
# verify_binary target — fail loudly if optimization level regressed
# (build/X/CMakeCache.txt with empty CMAKE_BUILD_TYPE → -O2 → broken)
# Run with: cmake --build <build_dir> --target verify_binary
# ─────────────────────────────────────────────────────────────────────
if(CURVE STREQUAL "BLS12_381")
    set(EXPECTED_TEXT_SIZE 22084)
    set(EXPECTED_BSS_SIZE  35984)
    set(EXPECTED_BIN_SIZE  22104)

    add_custom_target(verify_binary
        COMMAND ${CMAKE_COMMAND} -E echo "Verifying binary fingerprint..."
        COMMAND arm-none-eabi-size --format=berkeley $<TARGET_FILE:amore_bls12_381.elf>
        COMMAND ${CMAKE_COMMAND} -E echo "Expected: text=${EXPECTED_TEXT_SIZE} (Release/-O3 build)"
        COMMAND bash -c "
            T=$$(arm-none-eabi-size --format=berkeley $<TARGET_FILE:amore_bls12_381.elf> | awk 'NR==2 {print $$1}');
            B=$$(arm-none-eabi-size --format=berkeley $<TARGET_FILE:amore_bls12_381.elf> | awk 'NR==2 {print $$3}');
            if [ \"$$T\" -ne ${EXPECTED_TEXT_SIZE} ]; then
                echo '  ❌ FAIL: .text size mismatch'
                echo '     got      = '$$T' bytes'
                echo '     expected = ${EXPECTED_TEXT_SIZE} bytes (Release/-O3 build)'
                echo '     If got is ~14340, CMakeCache has empty CMAKE_BUILD_TYPE.'
                echo '     Fix: rm -rf build/CURVE/ and rebuild with -DCMAKE_BUILD_TYPE=Release'
                exit 1
            fi
            if [ \"$$B\" -ne ${EXPECTED_BSS_SIZE} ]; then
                echo '  ❌ FAIL: .bss size mismatch'
                echo '     got      = '$$B' bytes'
                echo '     expected = ${EXPECTED_BSS_SIZE} bytes'
                exit 1
            fi
            echo '  ✓ binary fingerprint OK (.text=${EXPECTED_TEXT_SIZE}, .bss=${EXPECTED_BSS_SIZE})'
        "
        DEPENDS amore_bls12_381.elf
        COMMENT "Verify ELF size/section fingerprint matches expected Release/-O3 build"
    )
endif()

# BN254 equivalent
if(CURVE STREQUAL "BN254")
    # TODO: populate after first verified BN254 -O3 build
    message(STATUS "verify_binary target not yet defined for BN254 — populate when verified")
endif()
