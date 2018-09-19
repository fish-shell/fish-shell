
#
# Completion for doas https://github.com/multiplexd/doas
# based on the sudo completions
#

function __fish_doas_print_remaining_args
    set -l tokens (commandline -opc) (commandline -ct)
    set -e tokens[1]
    # These are all the options mentioned in the man page for openbsd's "doas" (in that order).
    set -l opts a= C= L n s u=
    argparse -s $opts -- $tokens 2>/dev/null
    # The remaining argv is the subcommand with all its options, which is what
    # we want.
    if test -n "$argv"
        and not string match -qr '^-' $argv[1]
        echo $argv
        return 0
    else
        return 1
    end
end

function __fish_doas_no_subcommand
    not __fish_doas_print_remaining_args >/dev/null
end

complete -c doas -n "__fish_doas_no_subcommand" -s a -d "Choose auth method on openbsd systems"
complete -c doas -n "__fish_doas_no_subcommand" -s C -r -d "Parse and check given config file, then search for applying rules"
complete -c doas -n "__fish_doas_no_subcommand" -s L -d "Clear persisted authorizations, then immeadiately exit. No command is exectued"
complete -c doas -n "__fish_doas_no_subcommand" -s n -d "Non interactive mode, fail if doas would prompt for password."
complete -c doas -n "__fish_doas_no_subcommand" -s s -d "Execute the shell from SHELL or /etc/passwd."
complete -c doas -n "__fish_doas_no_subcommand" -s u -a "(__fish_complete_users)" -x -d "Execute the command as user. The default is root."

# Complete the command we are executing under doas
complete -c doas -xa "( __fish_complete_subcommand )"
