#RUN: fish_indent=%fish_indent %fish %s
#REQUIRES: command -v sphinx-build

set -l workspace_root (status dirname)/../..
set -l build_script $workspace_root/tests/test_functions/sphinx-shared.sh
# sphinx-build needs fish_indent in $PATH
set -lxp PATH (path dirname $fish_indent)
$build_script html
