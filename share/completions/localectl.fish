# Completion for systemd's localectl
set -l commands status set-locale list-locales set-keymap list-keymaps set-x11-keymap list-x11-keymap-{models,layouts,variants,options}

complete -c localectl -f
for cmd in $commands
    complete -c localectl -n "not __fish_seen_subcommand_from $commands" -a $cmd
end
set -l localevars LANG LC_MESSAGES LC_{CTYPE,NUMERIC,TIME,COLLATE,MONETARY,MESSAGES,PAPER,NAME,ADDRESS,TELEPHONE,MEASUREMENT,IDENTIFICATION,ALL}
set -l locales $localevars=(localectl list-locales 2>/dev/null)

function __fish_localectl_layout
    set -l cmd (commandline -pxc)
    set -e cmd[1]
    set -e cmd[1]
    for l in (localectl list-x11-keymap-layouts)
        if contains -- $l $cmd
            echo $l
            return 0
        end
    end
    return 1
end

complete -c localectl -n "__fish_seen_subcommand_from set-locale" -a "$locales"
complete -c localectl -n "__fish_seen_subcommand_from set-keymap" -a "(localectl list-keymaps)"
# set-x11-keymap takes layout model variant option... (i.e. multiple options)
complete -c localectl -n "__fish_seen_subcommand_from set-x11-keymap; and not __fish_seen_subcommand_from (localectl list-x11-keymap-layouts)" -a "(localectl list-x11-keymap-layouts)"
complete -c localectl -n "__fish_seen_subcommand_from set-x11-keymap; and __fish_seen_subcommand_from (localectl list-x11-keymap-layouts); and not __fish_seen_subcommand_from (localectl list-x11-keymap-models)" -a "(localectl list-x11-keymap-models)"
# Only complete variants for the current layout
complete -c localectl -n "__fish_seen_subcommand_from set-x11-keymap; and __fish_seen_subcommand_from (localectl list-x11-keymap-models); and not __fish_seen_subcommand_from (localectl list-x11-keymap-variants)" -a "(localectl list-x11-keymap-variants (__fish_localectl_layout))"
complete -c localectl -n "__fish_seen_subcommand_from set-x11-keymap; and __fish_seen_subcommand_from (localectl list-x11-keymap-variants)" -a "(localectl list-x11-keymap-options)"
complete -c localectl -l no-ask-password -d "Don't ask for password"
complete -c localectl -l no-convert -d "Don't convert keymap from console to X11 and vice-versa"
complete -c localectl -s H -l host -d 'Execute the operation on a remote host'
complete -c localectl -s h -l help -d 'Print a short help text and exit'
complete -c localectl -l version -d 'Print a short version string and exit'
complete -c localectl -l no-pager -d 'Do not pipe output into a pager'
