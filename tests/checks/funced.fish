#RUN: fish=%fish %fish %s

function my-src
    echo hello
end

echo "functions -q my-dst || functions --copy my-src my-dst" >my-copy-function.fish
source my-copy-function.fish

functions --details --verbose my-dst
# CHECK: my-copy-function.fish
# CHECK: n/a
# CHECK: 1
# CHECK: scope-shadowing

VISUAL=cat EDITOR=cat funced my-dst
# CHECK: functions -q my-dst || functions --copy my-src my-dst
# CHECK: Editor exited but the function was not modified
# CHECK: If the editor is still running, check if it waits for completion, maybe a '--wait' option?
