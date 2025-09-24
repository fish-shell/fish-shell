#RUN: %fish %s
#REQUIRES: command -v sphinx-build

set -l build_script (status dirname)/../test_functions/sphinx-shared.sh
$build_script man
