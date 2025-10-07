# RUN: %fish -d config | grep -v ^Debug.enabled
# REQUIRES: %fish -c 'status build-info' | grep '^Features:.*embed-data'

# CHECKERR: config: executable path: {{.*}}/fish
# CHECKERR: config: embed-data feature is active, ignoring data paths

# NOTE: When our executable is located outside the workspace, this is "/etc".
# CHECKERR: config: paths.sysconf: {{.+}}/etc

# CHECKERR: config: paths.bin: {{.*}}
# CHECKERR: config: sourcing {{.+}}/etc/config.fish
# CHECKERR: config: not sourcing {{.*}}/xdg_config_home/fish/config.fish (not readable or does not exist)
