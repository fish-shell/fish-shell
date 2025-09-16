#RUN: %fish %s
# Example output:
# Build system: CMake
# Version: 3.7.1-2573-gea8301631-dirty
# Target (and host): x86_64-unknown-linux-gnu
# Profile: release
# Features: gettext

status build-info | grep -v 'Host:'
# CHECK: Build system: {{CMake|Cargo}}
# CHECK: Version: {{.+}}
# (this could be "Target (and Host)" or "Target:" and a separate line "Host:")
# CHECK: Target{{.*}}: {{.+}}
# CHECK: Profile: {{release|debug}}
# CHECK: Features:{{.*}}

test "$(status build-info)" = "$(status buildinfo)"
or echo 'missing backwards-compatible version?'
