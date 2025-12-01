#RUN: %fish %s
#REQUIRES: command -v sphinx-build

set -l workspace_root (status dirname)/../..
set -l build_script $workspace_root/tests/test_functions/sphinx-shared.sh
$build_script man -D fish_help_sections_output=$PWD/help_sections
diff -u $workspace_root/share/help_sections $PWD/help_sections
