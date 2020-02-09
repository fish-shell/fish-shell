# Support for benchmarking fish.

ADD_CUSTOM_TARGET(benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/benchmarks/driver.sh $<TARGET_FILE:fish>
    USES_TERMINAL
)
