#RUN: fish_indent=%fish_indent fish=%fish %fish %s
#REQUIRES: command -v sphinx-build

# sphinx-build needs fish_indent in $PATH
set -l build_script (status dirname)/sphinx-shared.sh
set -lxp PATH (path dirname $fish_indent)
$build_script man
