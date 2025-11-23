# Note that when a completion file is sourced a new block scope is created so `set -l` works.
set -l __fish_status_all_commands \
    basename \
    build-info \
    current-command \
    current-commandline \
    current-filename \
    current-function \
    current-line-number \
    dirname \
    features \
    filename \
    fish-path \
    function \
    get-file \
    is-block \
    is-breakpoint \
    is-command-substitution \
    is-full-job-control \
    is-interactive \
    is-interactive-job-control \
    is-interactive-read \
    is-login \
    is-no-job-control \
    job-control \
    language \
    line-number \
    list-files \
    print-stack-trace \
    stack-trace \
    terminal \
    terminal-os \
    test-feature \
    test-terminal-feature

# These are the recognized flags.
complete -c status -s h -l help -d "Display help and exit"

# The "is-something" subcommands.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-login -d "Test if this is a login shell"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-interactive -d "Test if this is an interactive shell"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-interactive-read -d "Test if inside an interactive read builtin"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-command-substitution -d "Test if a command substitution is currently evaluated"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-block -d "Test if a code block is currently evaluated"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-breakpoint -d "Test if a breakpoint is currently in effect"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-no-job-control -d "Test if new jobs are never put under job control"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-interactive-job-control -d "Test if only interactive new jobs are put under job control"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-full-job-control -d "Test if all new jobs are put under job control"

# The subcommands that are not "is-something" which don't change the fish state.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a build-info -d "Print information on how this version fish was built"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-command -d "Print the name of the currently running command or function"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-commandline -d "Print the currently running command with its arguments"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-filename -d "Print the filename of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a filename -d "Print the filename of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a basename -d "Print the file name (without the path) of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a dirname -d "Print the path (without the file name) of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-function -d "Print the name of the current function"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a function -d "Print the name of the current function"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-line-number -d "Print the line number of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a line-number -d "Print the line number of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a print-stack-trace -d "Print a list of all function calls leading up to running the current command"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a stack-trace -d "Print a list of all function calls leading up to running the current command"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a features -d "List all feature flags"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a test-feature -d "Test if a feature flag is enabled"
complete -f -c status -n "__fish_seen_subcommand_from test-feature" -a '(status features | sed "s/[[:space:]]\+[^[:space:]]*[[:space:]]\+[^[:space:]]*/\t/")'
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a fish-path -d "Print the path to the current instance of fish"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a get-file -d "Print an embedded file from the fish binary"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a list-files -d "List embedded files contained in the fish binary"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a fish-path -d "Print the path to the current instance of fish"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a terminal -d "Print name and version of the terminal fish is running in"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a terminal-os -d "Print the operating system the terminal is running on"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a test-terminal-feature -d "Test if the terminal suports the given feature"
complete -f -c status -n "__fish_seen_subcommand_from test-terminal-feature" -a 'scroll-content-up\t"Command for scrolling up terminal contents"'

# The job-control command changes fish state.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a job-control -d "Set which jobs are under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a full -d "Set all jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a interactive -d "Set only interactive jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a none -d "Set no jobs under job control"

complete -f -c status -n "__fish_seen_subcommand_from get-file" -a '(status list-files 2>/dev/null)'
complete -f -c status -n "__fish_seen_subcommand_from list-files" -a '(status list-files 2>/dev/null)'

# Tests equality between the command line with the first item removed
# and the function's arguments.
function __fish_status_is_exact_subcommand
    set -l line (commandline -pxc)[2..]
    test "$line" = "$argv"
end
# Tests if the command line with the first item removed starts with the provided arguments.
function __fish_status_is_subcommand_prefix
    set -l prefix (string escape --style=regex -- (string join -- ' ' $argv))
    set -l line (string join -- ' ' (commandline -pxc)[2..])
    string match -rq -- "^$prefix" $line
end
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a language -d "Show or change fish's language settings"
complete -f -c status -n "__fish_status_is_exact_subcommand language" -a "(echo list-available\tShow languages usable with \'status language set\'\nset\tSet the language\(s\) used for fish\'s messages\nunset\tUndo effects of \'status language set\'\n)"
complete -f -c status -n "__fish_status_is_subcommand_prefix language set" -a "(status language list-available)"
