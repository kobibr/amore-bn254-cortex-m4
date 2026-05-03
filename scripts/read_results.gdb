# read_results.gdb
# GDB script that reads g_results from the MCU RAM

target extended-remote localhost:3333

monitor reset halt

# Print the address of g_results
echo \n=== AmorE Benchmark Results from RAM ===\n

# Read the entire struct
echo [STATUS]\n
p g_results.status
p g_results.magic

echo [SYSTEM]\n  
p g_results.core_mhz
p g_results.dwt_1ms

echo [SETUP]\n
p g_results.setup_cycles

echo [N=1]\n
p g_results.blind_1
p g_results.verify_1
p g_results.amort_1

echo [N=10]\n
p g_results.blind_10
p g_results.verify_10
p g_results.amort_10

echo [N=50]\n
p g_results.blind_50
p g_results.verify_50
p g_results.amort_50

echo [SECURITY]\n
p g_results.security_ok

monitor resume
quit
