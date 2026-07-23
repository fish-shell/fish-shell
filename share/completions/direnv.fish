set -l commands allow permit grant block deny revoke edit exec export fetchurl help hook prune reload status stdlib version

set -l with_file allow permit grant block deny revoke edit

complete -c direnv -f
complete -c direnv -n "__fish_seen_subcommand_from $with_file" -F

complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "allow permit grant" -d "Grant direnv permission to load the given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a "block deny revoke" -d "Revoke the authorization of a given .envrc or .env file"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a edit -d "Open given file or current .envrc/.env in \$EDITOR and allow it to be loaded afterwards"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a exec -d "Execute a command after loading the first .envrc or .env found in DIR"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a export -d "Load an .envrc/.env and print the diff in terms of exports"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a fetchurl -d "Fetch a given URL into direnv's CAS"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a help -d "show help"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a hook -d "Used to setup the shell hook"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a prune -d "Remove old allowed files"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a reload -d "Trigger an env reload"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a status -d "Print some debug status information"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a stdlib -d "Display the stdlib available in the .envrc execution context"
complete -c direnv -n "not __fish_seen_subcommand_from $commands" \
    -a version -d "Print the version or check that direnv is at least given version"

# List of valid shells/export formats for `direnv export` and `direnv hook` current as of direnv 2.37.1
# The combined list is from `direnv help`; the subset accepted by `hook` was tested empirically.
set -l shells "bash zsh fish tcsh elvish pwsh murex"
set -l additional_export_formats "gha gzenv json vim systemd"
complete -c direnv -n "__fish_seen_subcommand_from hook; and test (__fish_number_of_cmd_args_wo_opts) -eq 2" \
    -a "$shells" -d Shell
complete -c direnv -n "__fish_seen_subcommand_from export; and test (__fish_number_of_cmd_args_wo_opts) -eq 2" \
    -a "$shells $additional_export_formats" -d "Export format"

# Directory completions for the dir argument of `direnv exec DIR COMMAND [...ARGS]`
complete -c direnv -n "__fish_seen_subcommand_from exec; and test (__fish_number_of_cmd_args_wo_opts) -eq 2" \
    -a "(__fish_complete_directories)" -d "Directory to load .envrc from"

# Completions for the command or the args in `direnv exec DIR COMMAND [...ARGS]`
#
# Uses the PATH from inside the direnv environment when possible. Falls
# back to the normal PATH if it failed to find out the PATH inside the
# environment for any reason (nonexistent dir, `direnv allow` hasn't
# been run for the config file yet, etc)
function __direnv_exec_complete
    # `exec` is the first non-option token, DIR is the second.
    # At the moment, direnv treats an empty DIR argument as the current dir.
    set -l dir (__fish_nth_token 2) || return
    set -l dpath (direnv exec "$dir" sh -c 'printf %s "$PATH"' 2>/dev/null | string split :)
    set -q dpath[1]; or set dpath $PATH

    set -lx PATH $dpath
    __fish_complete_subcommand --fcs-skip=3
end
complete -c direnv -n "__fish_seen_subcommand_from exec; and test (__fish_number_of_cmd_args_wo_opts) -ge 3" \
    -xa "(__direnv_exec_complete)"
