# We don't want syntax errors to emit command usage help. This makes the
# stderr output considerably shorter and makes it easier to updates the tests
# and documentation without having to make pointless changes to the test
# output files.
function __fish_print_help
end
