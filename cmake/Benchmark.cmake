# Support for benchmarking fish.

add_custom_target(benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/benchmarks/driver.sh $<TARGET_FILE:fish>
    USES_TERMINAL
)
