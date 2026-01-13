#RUN: fish=%fish %fish %s

# Exact tag:
# 1.2.3
# Git version:
# 1.2.3-42-gdeadbeef
# Git version when no tags are present. Distros should probably not package it like this.
# 1.2.3-gdeadbeef

$fish -v
# CHECK: fish, version {{\d+\.\d+\.\d+(-\d+-g[0-9a-f]{7,}(-dirty)?|-g[0-9a-f]{7,})?$}}

set -l workspace_root (status dirname)/../..
$workspace_root/build_tools/git_version_gen.sh
# CHECK: {{\d+\.\d+\.\d+(-\d+-g[0-9a-f]{7,}(-dirty)?|-g[0-9a-f]{7,})?$}}
