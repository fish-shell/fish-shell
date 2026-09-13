# RUN: %fish %s

# Both pipeline elements use the captured function, even if it erases itself.
function cat
    functions -e cat
    command cat
    echo function
end
printf 'one\ntwo\n' | cat | cat
# CHECK: one
# CHECK: two
# CHECK: function
# CHECK: function

# Redefining a function also leaves the captured definition intact.
function captured
    function captured
        echo replacement
    end
    command cat
    echo original
end
printf 'input\n' | captured | captured
# CHECK: input
# CHECK: original
# CHECK: original
captured
# CHECK: replacement
