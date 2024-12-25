#RUN: %fish %s
# Example output:
# Build system: CMake
# Version: 3.7.1-2573-gea8301631-dirty
# Target (and host): x86_64-unknown-linux-gnu
# Profile: release
# Features: gettext

status buildinfo
# CHECK: Build system: {{CMake|Cargo}}
# CHECK: Version: {{.+}}
# CHECK: Target (and host): {{.+}}
# CHECK: Profile: {{release|debug}}
# CHECK: Features:{{.*}}
