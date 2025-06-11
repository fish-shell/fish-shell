#RUN: fish_indent=%fish_indent fish=%fish %fish %s
#REQUIRES: command -v sphinx-build

set -l build_script (status dirname)/../test_functions/sphinx-shared.sh
# sphinx-build needs fish_indent in $PATH
set -lxp PATH (path dirname $fish_indent)
$build_script html
