# RUN: %fish -d config | grep -v ^Debug.enabled
# REQUIRES: %fish -c 'status build-info' | grep '^Features:.*embed-data'

# CHECKERR: config: executable path: {{.*}}/fish
# CHECKERR: config: Running out of build directory, using paths relative to $CARGO_MANIFEST_DIR (/home/johannes/git/fish-shell)

# NOTE: When our executable is located outside the workspace, this is "/etc".
# CHECKERR: config: paths.sysconf: {{.+}}/etc

# CHECKERR: config: paths.bin: {{.*}}

# NOTE: When our executable is located outside the build directory, these are different.
# CHECKERR: config: paths.data: {{.*}}/share
# CHECKERR: config: paths.doc: {{.*}}/user_doc/html

# CHECKERR: config: sourcing {{.+}}/etc/config.fish
# CHECKERR: config: not sourcing {{.*}}/xdg_config_home/fish/config.fish (not readable or does not exist)
