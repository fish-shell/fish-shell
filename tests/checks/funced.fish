#RUN: %fish %s

function my-src
    echo hello
end

echo "functions --copy my-src my-dst" >my-copy-function.fish
source my-copy-function.fish
rm my-copy-function.fish # Cleanup

functions --details --verbose my-dst
# CHECK: my-copy-function.fish
# CHECK: {{.*}}tests/checks/funced.fish
# CHECK: 3
# CHECK: scope-shadowing

VISUAL=cat EDITOR=cat funced my-dst
# CHECK: # Defined in {{.*}}/tests/checks/funced.fish @ line 3, copied in my-copy-function.fish @ line 1
# CHECK: function my-dst
# CHECK:     echo hello
# CHECK: end
# CHECK: Editor exited but the function was not modified
# CHECK: If the editor is still running, check if it waits for completion, maybe a '--wait' option?

# Test: funced on a function copied in-script should open the copy's name, not the original's
function my-orig
    echo hello
end
functions --copy my-orig my-copied

functions --details my-copied
# CHECK: {{.*}}funced.fish

VISUAL=cat EDITOR=cat funced my-copied
# CHECK: # Defined in {{.*}}/funced.fish @ line {{.*}}, copied in {{.*}}/funced.fish @ line {{.*}}
# CHECK: function my-copied
# CHECK:     echo hello
# CHECK: end
# CHECK: Editor exited but the function was not modified
# CHECK: If the editor is still running, check if it waits for completion, maybe a '--wait' option?
