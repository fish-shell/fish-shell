# Support for benchmarking fish.

add_custom_target(benchmark
    COMMAND ${CMAKE_SOURCE_DIR}/benchmarks/driver.sh ${CMAKE_BINARY_DIR}/fish
    DEPENDS fish
    USES_TERMINAL
)
