# Support for benchmarking ghoti.

add_custom_target(benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/benchmarks/driver.sh $<TARGET_FILE:ghoti>
    USES_TERMINAL
)
