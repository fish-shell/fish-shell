# Completion for doas https://github.com/multiplexd/doas
# based on the sudo completions
#

function __fish_doas_print_remaining_args
    set -l tokens (commandline -xpc | string escape) (commandline -ct)
    set -e tokens[1]
    # These are all the options mentioned in the man page for openbsd's "doas" (in that order).
    set -l opts a= C= L n s u=
    argparse -s $opts -- $tokens 2>/dev/null
    # The remaining argv is the subcommand with all its options, which is what
    # we want.
    if test -n "$argv"
        and not string match -qr '^-' $argv[1]
        string join0 -- $argv
        return 0
    else
        return 1
    end
end

function __fish_complete_doas_subcommand
    set -l args (__fish_doas_print_remaining_args | string split0)
    set -lx -a PATH /usr/local/sbin /sbin /usr/sbin
    __fish_complete_subcommand --commandline $args
end

complete -c doas -n "not __fish_doas_print_remaining_args" -s a -d "Choose auth method on systems using /etc/login.conf"
complete -c doas -n "not __fish_doas_print_remaining_args" -s C -r -d "validate given config file and test it against given command"
complete -c doas -n "not __fish_doas_print_remaining_args" -s L -d "Clear persisted authorizations, then exit"
complete -c doas -n "not __fish_doas_print_remaining_args" -s n -d "Fail if doas would prompt for password"
complete -c doas -n "not __fish_doas_print_remaining_args" -s s -d "Execute the shell from SHELL or /etc/passwd"
complete -c doas -n "not __fish_doas_print_remaining_args" -s u -a "(__fish_complete_users)" -x -d "Execute the command as user. The default is root."

# Complete the command we are executing under doas
complete -c doas -x -a "(__fish_complete_doas_subcommand)"
