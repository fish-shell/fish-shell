#RUN: %fish %s

set -g fish_complete_path c1 c2
mkdir c1 c2

function foo; end
for i in c1 c2
    echo >$i/foo.fish "echo auto-loading $i/foo.fish"
end
complete -C "foo " >/dev/null
# CHECK: auto-loading c1/foo.fish
complete -C "foo " >/dev/null
# already loaded

set -g fish_complete_path c2
complete -C "foo " >/dev/null
# CHECK: auto-loading c2/foo.fish

set -g fish_complete_path c1 c2
complete -C "foo " >/dev/null
# CHECK: auto-loading c1/foo.fish
