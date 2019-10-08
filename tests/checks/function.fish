#RUN: %fish %s
function t --argument-names a b c
    echo t
end

function t2 --argument-names a b c --no-scope-shadowing
    echo t2
end
#CHECKERR: {{.*/?}}function.fish (line {{\d+}}): function: Variable name '--no-scope-shadowing' is not valid. See `help identifiers`.
#CHECKERR: function t2 --argument-names a b c --no-scope-shadowing
#CHECKERR: ^

functions -q t2 && echo exists || echo does not exist
#CHECK: does not exist
