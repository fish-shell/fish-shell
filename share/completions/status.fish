# Note that when a completion file is sourced a new block scope is created so `set -l` works.
set -l __fish_status_all_commands \
    basename \
    buildinfo \
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
    line-number \
    print-stack-trace \
    stack-trace \
    test-feature

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

# The job-control command changes fish state.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a job-control -d "Set which jobs are under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a full -d "Set all jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a interactive -d "Set only interactive jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a none -d "Set no jobs under job control"
