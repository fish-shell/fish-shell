# RUN: %fish %s
# REQUIRES: command -v man

# Test redirecting builtin help with a pipe
# help should be redirected to grep instead of appearing on STDOUT
builtin and --help 2>| grep -q "Documentation for and"
echo $status
#CHECK: 0

# A missing manual must be reported even when its directory exists.
source "$(status dirname)/../../share/functions/__fish_print_help.fish"
function __fish_data_with_directory
    $argv[3..] "$PWD"
end
builtin and --help
# CHECKERR: and: missing man page
# CHECKERR: Documentation may not be installed.
# CHECKERR: `help and` will show an online version

# Online help must recognize commands without installed manual pages.
source "$(status dirname)/../../share/functions/__fish_print_commands.fish"
function __fish_man1_pages
end
fish_help_browser=echo help path | string match '*://*/cmds/path.html'
# CHECK: {{.*}}/cmds/path.html
