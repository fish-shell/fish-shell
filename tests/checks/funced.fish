#RUN: fish=%fish %fish %s

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

echo "function my-orig; echo original; end" >my-orig.fish
echo "function my-copy; echo edited; end" >my-copy-edited.fish
$fish -c '
    source my-orig.fish
    functions --copy my-orig my-copy
    echo (functions --details --verbose my-copy)[2]
    funced --editor "cp my-copy-edited.fish" --save my-copy
    my-copy
'
# CHECK: my-orig.fish
# CHECK: funcsave: wrote {{.*}}/fish/functions/my-copy.fish
# CHECK: edited
cat my-orig.fish
# CHECK: function my-orig; echo original; end
rm my-orig.fish my-copy-edited.fish # Cleanup
