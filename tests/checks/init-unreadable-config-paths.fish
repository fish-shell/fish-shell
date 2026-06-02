# RUN: fish=%fish %fish %s

mkdir config-home data-home
chmod 000 config-home data-home
XDG_CONFIG_HOME=$PWD/config-home \
    XDG_DATA_HOME=$PWD/data-home \
    $fish -c true
# CHECKERR: error: can not save history
# CHECKERR: warning-path: Unable to locate data directory derived from $XDG_DATA_HOME: '{{.*}}/data-home/fish'.
# CHECKERR: warning-path: The error was 'Permission denied (os error 13)'.
# CHECKERR: warning-path: Please set $XDG_DATA_HOME to a directory where you have write access.

# CHECKERR: error: can not save universal variables or functions
# CHECKERR: warning-path: Unable to locate config directory derived from $XDG_CONFIG_HOME: '{{.*}}/config-home/fish'.
# CHECKERR: warning-path: The error was 'Permission denied (os error 13)'.
# CHECKERR: warning-path: Please set $XDG_CONFIG_HOME to a directory where you have write access.
