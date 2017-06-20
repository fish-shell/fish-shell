# Note that when a completion file is sourced a new block scope is created so `set -l` works.
set -l __fish_status_all_commands is-login is-interactive is-block is-breakpoint is-command-substitution is-no-job-control is-interactive-job-control is-full-job-control current-filename current-line-number print-stack-trace job-control

# These are the recognized flags.
complete -c status -s h -l help --description "Display help and exit"

# The "is-something" subcommands.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-login -d "Test if this is a login shell"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-interactive -d "Test if this is an interactive shell"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-command-substitution -d "Test if a command substitution is currently evaluated"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-block -d "Test if a code block is currently evaluated"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-breakpoing -d "Test if a breakpoint is currently in effect"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-no-job-control -d "Test if new jobs are never put under job control"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-interactive-job-control -d "Test if only interactive new jobs are put under job control"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a is-full-job-control -d "Test if all new jobs are put under job control"

# The subcommands that are not "is-something" which don't change the fish state.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-filename -d "Print the filename of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a current-line-number -d "Print the line number of the currently running script"
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a print-stack-trace -d "Print a list of all function calls leading up to running the current command"

# The job-control command changes fish state.
complete -f -c status -n "not __fish_seen_subcommand_from $__fish_status_all_commands" -a job-control -d "Set which jobs are under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a full -d "Set all jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a interactive -d "Set only interactive jobs under job control"
complete -f -c status -n "__fish_seen_subcommand_from job-control" -a none -d "Set no jobs under job control"
