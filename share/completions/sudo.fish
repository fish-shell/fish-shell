#
# Completion for sudo
#

function __fish_sudo_print_remaining_args
    set -l tokens (commandline -xpc | string escape) (commandline -ct)
    set -e tokens[1]
    # These are all the options mentioned in the man page for Todd Miller's "sudo.ws" sudo (in that order).
    # If any other implementation has different options, this should be harmless, since they shouldn't be used anyway.
    set -l opts A/askpass b/background C/close-from= E/preserve-env='?'
    # Note that "-h" is both "--host" (which takes an option) and "--help" (which doesn't).
    # But `-h` as `--help` only counts when it's the only argument (`sudo -h`),
    # so any argument completion after that should take it as "--host".
    set -a opts e/edit g/group= H/set-home h/host= 1-help
    set -a opts i/login K/remove-timestamp k/reset-timestamp l/list n/non-interactive
    set -a opts P/preserve-groups p/prompt= S/stdin s/shell U/other-user=
    set -a opts u/user= T/command-timeout= V/version v/validate
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

function __fish_sudo_no_subcommand
    not __fish_sudo_print_remaining_args >/dev/null
end

function __fish_complete_sudo_subcommand
    set -l args (__fish_sudo_print_remaining_args | string split0)
    set -lx -a PATH /usr/local/sbin /sbin /usr/sbin
    __fish_complete_subcommand --commandline $args
end

# All these options should be valid for GNU and OSX sudo
complete -c sudo -n __fish_no_arguments -s h -d "Display help and exit"
complete -c sudo -n __fish_no_arguments -s V -d "Display version information and exit"
complete -c sudo -n __fish_sudo_no_subcommand -s A -d "Ask for password via the askpass or \$SSH_ASKPASS program"
complete -c sudo -n __fish_sudo_no_subcommand -s C -d "Close all file descriptors greater or equal to the given number" -xa "0 1 2 255"
complete -c sudo -n __fish_sudo_no_subcommand -s E -d "Preserve environment"
complete -c sudo -n __fish_sudo_no_subcommand -s H -d "Set home"
complete -c sudo -n __fish_sudo_no_subcommand -s K -d "Remove the credential timestamp entirely"
complete -c sudo -n __fish_sudo_no_subcommand -s P -d "Preserve group vector"
complete -c sudo -n __fish_sudo_no_subcommand -s S -d "Read password from stdin"
complete -c sudo -n __fish_sudo_no_subcommand -s b -d "Run command in the background"
complete -c sudo -n __fish_sudo_no_subcommand -s e -rF -d Edit
complete -c sudo -n __fish_sudo_no_subcommand -s g -a "(__fish_complete_groups)" -x -d "Run command as group"
complete -c sudo -n __fish_sudo_no_subcommand -s i -d "Run a login shell"
complete -c sudo -n __fish_sudo_no_subcommand -s k -d "Reset or ignore the credential timestamp"
complete -c sudo -n __fish_sudo_no_subcommand -s l -d "List the allowed and forbidden commands for the given user"
complete -c sudo -n __fish_sudo_no_subcommand -s n -d "Do not prompt for a password - if one is needed, fail"
complete -c sudo -n __fish_sudo_no_subcommand -s p -d "Specify a custom password prompt"
complete -c sudo -n __fish_sudo_no_subcommand -s s -d "Run the given command in a shell"
complete -c sudo -n __fish_sudo_no_subcommand -s u -a "(__fish_complete_users)" -x -d "Run command as user"
complete -c sudo -n __fish_sudo_no_subcommand -s v -n __fish_no_arguments -d "Validate the credentials, extending timeout"

# Complete the command we are executed under sudo
complete -c sudo -x -n 'not __fish_seen_argument -s e' -a "(__fish_complete_sudo_subcommand)"
