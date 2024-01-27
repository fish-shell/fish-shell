function __fish_watchexec_print_remaining_args
    set -l spec w/watch= c/clear='?' o/on-busy-update= r/restart s/signal= stop-signal= stop-timeout= d/debounce= stdin-quit no-vcs-ignore no-project-ignore no-global-ignore no-default-ignore no-discover-ignore p/postpone delay-run= poll= shell= n no-environment emit-events-to= E/env= no-process-group N/notify project-origin= workdir= e/exts= f/filter= filter-file= i/ignore= ignore-file= fs-events= no-meta print-events v/verbose log-file= manual h/help V/version

    set argv (commandline -xpc | string escape) (commandline -ct)
    set -e argv[1]

    argparse -s $spec -- $argv 2>/dev/null

    # The remaining argv is the subcommand with all its options, which is what
    # we want.
    if set -q argv[1]
        and not string match -qr '^-.*' -- $argv[1]
        string join0 -- $argv
        return 0
    else
        return 1
    end
end

function __fish_watchexec_complete_subcommand
    set -l args (__fish_watchexec_print_remaining_args | string split0)
    complete -C "$args"
end

function __fish_watchexec_at_argfile
    set -l current (commandline -ct)
    if test (count (commandline -xpc)) -eq 1
        and string match -q '@*' -- $current

        set current (string sub -s 2 -- $current)
        __fish_complete_path | string replace -r "^" @
        return 0
    end
    return 1
end

complete -c watchexec -n __fish_watchexec_at_argfile -x -a "(__fish_watchexec_at_argfile)"

complete -c watchexec -x -a "(__fish_watchexec_complete_subcommand)"

complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sw -lwatch -r -d "Watch a specific file or directory"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sc -lclear -fa reset -d "Clear screen before running command"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -so -lon-busy-update -r -xa "queue do-nothinig restart signal" -d "What to do when receiving events while the command is running"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sr -lrestart -d "Restart the process if it's still running"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -ss -lsignal -r -f -d "Send a signal to the process when it's still running"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lstop-signal -r -f -d "Signal to send to sstop the command"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lstop-timeout -r -f -d "Time to wait for the command to exit gracefully"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sd -ldebounce -r -f -d "Time to wait for new events before taking action"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lstdin-quit -d "Exit when stding closes"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-vcs-ignore -d "Don't load gitignores"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-project-ignore -d "Don't load project-local ignores"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-global-ignore -d "Don't load global ignores"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-default-ignore -d "Don't use internal default ignores"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-discover-ignore -d "Don't discover ignore files at all"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sp -lpostpone -d "Wait until first change before running command"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -ldelay-run -r -f -d "Sleep before running the command"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lpoll -r -f -d "Poll for filesystem changes"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lshell -r -fa "(printf 'none\tDon\'t use a shell\n';string match -r '^[^#].*' < /etc/shells)" -d "Use a different shell"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -ln -d "Don#t use a shell"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-environment -d "--emit-events=none"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lemit-events-to -r -fa "environment stdin file json-stdin json-file none" -d "Configure event emission"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sE -lenv -r -f -d "Add env vars to the command"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-process-group -d "Don't use a process group"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sN -lnotify -d "Alert when commands start and end"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lproject-origin -r -fa "(__fish_complete_directories)" -d "Set the project origin"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lworkdir -r -fa "(__fish_complete_directories)" -d "Set the working directory"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -se -lexts -r -f -d "Filename extensions to filter to"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sf -lfilter -r -f -d "Filename patterns to filter to"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lfilter-file -r -d "Files to load filters from"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -si -lignore -r -f -d "Filename patterns to filter out"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lignore-file -r -d "Files to load ignores from"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lfs-events -r -fa "access create remove rename modify metadata" -d "Filesystem events to filter to"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lno-meta -d "Filesystem events to filter to"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lprint-events -d "Print events that trigger actions"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sv -lverbose -d "Set diagnostic log level"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -llog-file -r -d "Write diagnostic logs to a file"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -lmanual -d "Show the manual page"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sh -lhelp -d "Print help (see a summary with '-h')"
complete -c watchexec -n "not __fish_watchexec_print_remaining_args" -sV -lversion -d "Print version"
