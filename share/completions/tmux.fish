function __fish_tmux_sessions -d 'available sessions'
    tmux list-sessions -F "#S"\t"#{session_windows} windows created: #{session_created_string} [#{session_width}x#{session_height}]#{session_attached}" | sed 's/0$//;s/1$/ (attached)/' 2>/dev/null
end

function __fish_tmux_clients -d 'connected clients'
    tmux list-clients -F "#{client_tty}"\t"#S: Created: #{client_created_string} [#{client_width}x#{client_height} #{client_termname}]" 2>/dev/null
end

function __fish_tmux_panes -d 'window panes'
    #fully qualified pane names
    tmux list-panes -F '#S:#W.#P'\t'session:window.pane' 2>/dev/null

    #panes by themselves
    tmux list-panes -F '#P'\t'pane' 2>/dev/null

    #windows by themselves
    tmux list-panes -F '#W'\t'window' 2>/dev/null
end

#don't allow dirs in the completion list...
complete -c tmux -x

###############  Begin: Dynamic Completions Using `tmux list-commands` ###############

# The dynamic completions are exhaustive. The manual completions below override
# them with better-documented completions and custom completions for some
# arguments, e.g. for target panes, but only have partial coverage.
function __fish_tmux_parse_lscm_usage
    set -l lscm (tmux list-commands -F "#{command_list_name} #{command_list_alias}"\t"#{command_list_usage}" 2>/dev/null)
    or return

    for cmd_tab_usage in $lscm
        set -l split_command (string split --max 2 \t -- $cmd_tab_usage)
        set -l cmdnames $split_command[1]
        set -l usage $split_command[2]

        complete -c tmux -n __fish_use_subcommand -a "$cmdnames"

        # $usage has the form '[-ABCD] [-L|-S|-U] [-e arg-name] [positional-arg-name]'
        for item in (string match -ag --regex '\[([^\]]+)\]' $usage)
            if not set -l item (string trim -l -c - -- $item)
                continue
            end
            if set -l split_item (string split -n --max 2 " " -- "$item")
                # The option should always have length 1 by tmux convention,
                # but we double-check to avoid syntax errors.
                if [ "$(string length $split_item[1])" = 1 ]
                    complete -c tmux -xs $split_item[1] -n "__fish_seen_subcommand_from $cmdnames" -d "$split_item[2]"
                end
            else
                for char in (string split '' -- "$item")
                    if string match -q -r '\||-' -- $char
                        # TODO: Actually treat exclusive args, [-L|-S|-U], as exclusive
                        # For now, we just ignore the `-`s and `|`s
                        continue
                    end
                    complete -c tmux -n "__fish_seen_subcommand_from $cmdnames" -s $char
                end
            end
        end
    end
end

__fish_tmux_parse_lscm_usage
functions -e __fish_tmux_parse_lscm_usage

# Completions for `tmux list-commands` itself
set -l all_commands (tmux list-commands -F "#{command_list_name} #{command_list_alias}" 2>/dev/null)
and complete -c tmux -n "__fish_seen_subcommand_from list-commands lscm" -x -a "$all_commands"

###############  End:   Dynamic Completions Using `tmux list-commands` ###############

###############  Begin: Front  Flags ###############
#these do not require parameters
complete -c tmux -n __fish_use_subcommand -s 2 -d 'Force tmux to assume the terminal supports 256 colours'
complete -c tmux -n __fish_use_subcommand -s 8 -d 'Like -2, but indicates that the terminal supports 88 colours'
complete -c tmux -n __fish_use_subcommand -s l -d 'Behave as a login shell'
complete -c tmux -n __fish_use_subcommand -s q -d 'Set the quiet server option'
complete -c tmux -n __fish_use_subcommand -s u -d 'Flag explicitly informs tmux that UTF-8 is supported'
complete -c tmux -n __fish_use_subcommand -s v -d 'Request verbose logging'
complete -c tmux -n __fish_use_subcommand -s V -d 'Report the tmux version'

#these require parameters
complete -c tmux -n __fish_use_subcommand -xs c -d 'Execute command using the default shell'
complete -c tmux -n __fish_use_subcommand -rs f -d 'Alternate config file'
complete -c tmux -n __fish_use_subcommand -rs L -d 'Specify the name of the server socket to use'
complete -c tmux -n __fish_use_subcommand -rs S -d 'Full path to server socket. If set, -L is ignored.'
###############  End:   Front  Flags ###############

###############  Begin: Clients and Sessions ###############
set -l attach 'attach-session attach at a'
set -l detach 'detach-client detach'
set -l has 'has-session has'
set -l killserver kill-server
set -l killsession kill-session
set -l lsc 'list-clients lsc'
set -l lscm 'list-commands lscm'
set -l ls 'list-sessions ls'
set -l lockc 'lock-client lockc'
set -l locks 'lock-session locks'
set -l new 'new-session new'
set -l refresh 'refresh-client refresh'
set -l rename 'rename-session rename'
set -l showmsgs 'show-messages showmsgs'
set -l source 'source-file source'
set -l start 'start-server start'
set -l suspendc 'suspend-client suspendc'
set -l switchc 'switch-client switchc'

complete -c tmux -n __fish_use_subcommand -a $attach -d 'attach to existing session'
complete -c tmux -n "__fish_seen_subcommand_from $attach" -s d -d 'detach other clients'
complete -c tmux -n "__fish_seen_subcommand_from $attach" -s r -d 'attach in read-only mode'

complete -c tmux -n __fish_use_subcommand -a $detach -d 'detach current client'
complete -c tmux -n "__fish_seen_subcommand_from $detach" -s P -d 'SIGHUP parent process of client, likely causing it to exit'

complete -c tmux -n __fish_use_subcommand -a $has -d 'report error and exit with 1 if the session does not exist'
complete -c tmux -n __fish_use_subcommand -a $killserver -d 'kill tmux server, clients, and sessions'
complete -c tmux -n __fish_use_subcommand -a $killsession -d 'destroy session, close its windows, and detach all its clients'
complete -c tmux -n __fish_use_subcommand -a $lsc -d 'list all attached clients'
complete -c tmux -n __fish_use_subcommand -a $lscm -d 'list syntax for all tmux commands'
complete -c tmux -n __fish_use_subcommand -a $ls -d 'list all sessions'
complete -c tmux -n __fish_use_subcommand -a $lockc -d 'lock client'
complete -c tmux -n __fish_use_subcommand -a $locks -d 'lock session'

complete -c tmux -n __fish_use_subcommand -a $new -d 'create a new session with name session-name'
complete -c tmux -n "__fish_seen_subcommand_from $new" -s d -d "don't attach to current window"
complete -c tmux -n "__fish_seen_subcommand_from $new" -s A -d "attach to existing session if session-name already exists"
complete -c tmux -n "__fish_seen_subcommand_from $new" -s D -d "if -A is specified, detach other clients attached to the session"
complete -c tmux -n "__fish_seen_subcommand_from $new" -s P -d "print information about the new session after creation"
complete -c tmux -n "__fish_seen_subcommand_from $new" -s E -d "don't apply update-environment option"
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs c -d start-directory
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs F -d format
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs n -d window-name
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs s -d session-name
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs t -d group-name
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs x -d width
complete -c tmux -n "__fish_seen_subcommand_from $new" -xs y -d height

complete -c tmux -n __fish_use_subcommand -a $refresh -d 'refresh client'
complete -c tmux -n "__fish_seen_subcommand_from $refresh" -s S -d 'update client status bar'

complete -c tmux -n __fish_use_subcommand -a $rename -d 'rename session'
complete -c tmux -n __fish_use_subcommand -a $showmsgs -d 'save msgs in status bar in per-client msg log'

complete -c tmux -n __fish_use_subcommand -a $source -d 'execute commands from path'
complete -c tmux -n "__fish_seen_subcommand_from $source" -F

complete -c tmux -n __fish_use_subcommand -a $start -d 'start tmux server if not running; do not create a session'

complete -c tmux -n __fish_use_subcommand -a $suspendc -d 'send SIGTSTP signal to client (tty stop)'

complete -c tmux -n __fish_use_subcommand -a $switchc -d 'Switch the current session for client target-client to target-session'
complete -c tmux -n "__fish_seen_subcommand_from $switchc" -s l -d 'move client to the last session'
complete -c tmux -n "__fish_seen_subcommand_from $switchc" -s n -d 'move client to the next session'
complete -c tmux -n "__fish_seen_subcommand_from $switchc" -s p -d 'move client to the previous session'
complete -c tmux -n "__fish_seen_subcommand_from $switchc" -s r -d 'toggle if client is read-only'

#commands with a session flag
complete -c tmux -xs t -n "__fish_seen_subcommand_from $attach $new $has $killsession $lsc $locks $rename $switchc" -a '(__fish_tmux_sessions)' -d target-session
complete -c tmux -xs s -n "__fish_seen_subcommand_from $detach" -a '(__fish_tmux_sessions)' -d target-session

#commands with a client flag
complete -c tmux -xs t -n "__fish_seen_subcommand_from $detach $lockc $refresh $showmsgs $suspendc" -a '(__fish_tmux_clients)' -d target-client
complete -c tmux -xs c -n "__fish_seen_subcommand_from $switchc" -a '(__fish_tmux_clients)' -d target-client

#commands with the -F format flag
complete -c tmux -n "__fish_seen_subcommand_from $lsc $ls" -xs F -d 'format string'

#commands that take shell commands
complete -c tmux -x -n "__fish_seen_subcommand_from $new " -a '(__fish_complete_subcommand --fcs-skip=2)'

###############  End: Clients and Sessions ###############

###############  Begin: Windows and Panes ###############

set -l breakp "break-pane breakp"
set -l capturep "capture-pane capturep"
set -l chooseclient choose-client
set -l choosetree choose-tree
set -l customizemode customize-mode
set -l displayp "display-panes displayp"
set -l findw "find-window findw"
set -l joinp "join-pane joinp move-pane movep"
set -l killp "kill-pane killp"
set -l killw "kill-window killw"
set -l lastp "last-pane lastp"
set -l lastw "last-window lastw"
set -l linkw "link-window linkw"
set -l lsp "list-panes lsp"
set -l lsw "list-windows lsw"
set -l movew "move-window movew"
set -l neww "new-window neww"
set -l nextl "next-layout nextl"
set -l next "next-window next"
set -l pipep "pipe-pane pipep"
set -l prevl "previous-layout prevl"
set -l prev "previous-window prev"
set -l renamew "rename-window renamew"
set -l resizep "resize-pane resizep"
set -l resizew "resize-window resizew"
set -l respawnp "respawn-pane respawnp"
set -l respawnw "respawn-window respawnw"
set -l rotatew "rotate-window rotatew"
set -l selectl "select-layout selectl"
set -l selectp "select-pane selectp"
set -l selectw "select-window selectw"
set -l splitw "split-window splitw"
set -l swapp "swap-pane swapp"
set -l swapw "swap-window swapw"
set -l unlinkw "unlink-window unlinkw"

complete -c tmux -n __fish_use_subcommand -a $breakp -d 'break pane off into a new window'

complete -c tmux -n __fish_use_subcommand -a $capturep -d 'capture contents of a pane into a buffer'
complete -c tmux -n "__fish_seen_subcommand_from $capturep" -xs a -d 'capture alternate screen'
complete -c tmux -n "__fish_seen_subcommand_from $capturep" -xs p -d 'output to stdout'
complete -c tmux -n "__fish_seen_subcommand_from $capturep" -xs e -d 'include color escapes'
complete -c tmux -n "__fish_seen_subcommand_from $capturep" -xs C -d 'escape non-printable chars'

complete -c tmux -n __fish_use_subcommand -a $chooseclient -d 'interactively choose client'
complete -c tmux -n __fish_use_subcommand -a $choosetree -d 'interactively choose session/window/pane'
complete -c tmux -n __fish_use_subcommand -a $customizemode -d 'interactively customize settings'
complete -c tmux -n __fish_use_subcommand -a $displayp -d 'display a visible indicator for each pane'
complete -c tmux -n __fish_use_subcommand -a $findw -d 'interactively choose window matching pattern'

complete -c tmux -n __fish_use_subcommand -a $joinp -d 'split destination pane and move source pane into one of the halves'
# $joinp takes a subset of $splitw arguments
complete -c tmux -n "__fish_seen_subcommand_from $joinp $splitw" -xs b -d 'before target'
complete -c tmux -n "__fish_seen_subcommand_from $joinp $splitw" -xs h -d 'horizontal'
complete -c tmux -n "__fish_seen_subcommand_from $joinp $splitw" -xs v -d 'vertical'
complete -c tmux -n "__fish_seen_subcommand_from $joinp $splitw" -xs l -d 'size in lines/cols'
complete -c tmux -n "__fish_seen_subcommand_from $joinp $splitw" -xs f -d 'full height/width'

complete -c tmux -n __fish_use_subcommand -a $killp -d 'destroy a pane'
complete -c tmux -n __fish_use_subcommand -a $killw -d 'destroy a window'
complete -c tmux -n __fish_use_subcommand -a $lastp -d 'select the previusly selected pane'
complete -c tmux -n __fish_use_subcommand -a $lastw -d 'select the previusly selected window'
complete -c tmux -n __fish_use_subcommand -a $linkw -d 'link source window to destination window'

complete -c tmux -n __fish_use_subcommand -a $lsp -d 'list panes'
complete -c tmux -n "__fish_seen_subcommand_from $lsp" -xs s -d 'all in session'

complete -c tmux -n __fish_use_subcommand -a $lsw -d 'list windows'
complete -c tmux -n __fish_use_subcommand -a $movew -d 'move window'

complete -c tmux -n __fish_use_subcommand -a $neww -d 'create a new window'
complete -c tmux -n "__fish_seen_subcommand_from $neww" -xs k -d 'replace if exists'
complete -c tmux -n "__fish_seen_subcommand_from $neww" -xs S -d 'select if exists'

complete -c tmux -n __fish_use_subcommand -a $nextl -d 'rearrange panes in a window according to the next layout'
complete -c tmux -n __fish_use_subcommand -a $next -d 'move to the next window in the session'
complete -c tmux -n __fish_use_subcommand -a $pipep -d 'pipe output from pane to a shell command'
complete -c tmux -n __fish_use_subcommand -a $prevl -d 'rearrange panes in a window according to the previous layout'
complete -c tmux -n __fish_use_subcommand -a $prev -d 'move to the previous window in the session'
complete -c tmux -n __fish_use_subcommand -a $renamew -d 'rename a window'

complete -c tmux -n __fish_use_subcommand -a $resizep -d 'resize a pane'
complete -c tmux -n "__fish_seen_subcommand_from $resizep" -xs M -d 'begin mouse resize'
complete -c tmux -n "__fish_seen_subcommand_from $resizep" -xs T -d 'trim below cursor'

complete -c tmux -n __fish_use_subcommand -a $resizew -d 'resize a window'
complete -c tmux -n __fish_use_subcommand -a $respawnp -d 'reactivate a pane where a command exited'
complete -c tmux -n __fish_use_subcommand -a $respawnw -d 'reactivate a window where a command exited'
complete -c tmux -n __fish_use_subcommand -a $rotatew -d 'rotate panes within a window'

complete -c tmux -n __fish_use_subcommand -a $selectl -d 'rearrange panes according to a given layout'
set -l layouts 'even-horizontal even-vertical main-horizontal main-horizontal-mirrored main-vertical main-vertical-mirrored tiled'
complete -c tmux -n "__fish_seen_subcommand_from $selectl" -x -a "$layouts" -d 'predefined layout'

complete -c tmux -n __fish_use_subcommand -a $selectp -d 'activate specific pane'
complete -c tmux -n "__fish_seen_subcommand_from $selectp" -xs d -d 'disable input'
complete -c tmux -n "__fish_seen_subcommand_from $selectp" -xs e -d 'enable input'
complete -c tmux -n "__fish_seen_subcommand_from $selectp" -xs l -d 'previously selected'
complete -c tmux -n "__fish_seen_subcommand_from $selectp" -xs m -d 'mark'
complete -c tmux -n "__fish_seen_subcommand_from $selectp" -xs M -d 'unmark'

complete -c tmux -n __fish_use_subcommand -a $selectw -d 'activate specific window'
complete -c tmux -n "__fish_seen_subcommand_from $selectw" -xs l -d 'previously selected'
complete -c tmux -n "__fish_seen_subcommand_from $selectw" -xs p -d 'previous'
complete -c tmux -n "__fish_seen_subcommand_from $selectw" -xs n -d 'next'

complete -c tmux -n __fish_use_subcommand -a $splitw -d 'create a new pane by splitting target-pane'
# See also $joinp's arguments
complete -c tmux -n "__fish_seen_subcommand_from $splitw" -xs I -d 'show stdin contents'

complete -c tmux -n __fish_use_subcommand -a $swapp -d 'swap two panes'
complete -c tmux -n __fish_use_subcommand -a $swapw -d 'swap two windows'
complete -c tmux -n __fish_use_subcommand -a $unlinkw -d 'unlink target-window'

## commands with pane flag
complete -c tmux -n "__fish_seen_subcommand_from $breakp $joinp $swapp" -xs s -a '(__fish_tmux_panes)' -d 'source pane'
complete -c tmux -n "__fish_seen_subcommand_from $capturep $chooseclient $choosetree $customizemode $findw" -xs t -a '(__fish_tmux_panes)' -d 'target pane'
complete -c tmux -n "__fish_seen_subcommand_from $killp $pipep $resizep $respawnp $selectl $selectp $splitw" -xs t -a '(__fish_tmux_panes)' -d 'target pane'
# Unclear if there's a meaningful difference between "target pane" and "destination pane", but tmux makes the distinction
complete -c tmux -n "__fish_seen_subcommand_from $joinp $swapp" -xs t -a '(__fish_tmux_panes)' -d 'destination pane'

## commands with session flag
complete -c tmux -n "__fish_seen_subcommand_from $lastw $lsw $next $prev" -xs t -a '(__fish_tmux_sessions)' -d 'target session'

## commands with the -F format flag
complete -c tmux -n "__fish_seen_subcommand_from $breakp $lsp $lsw $neww $chooseclient $choosetree" -xs F -d 'format string'

## commands with -s/-t flags that are not panes/sessions (nice completion not yet implemented)
complete -c tmux -n "__fish_seen_subcommand_from $linkw $movew $swapw" -xs s -d 'source window'
complete -c tmux -n "__fish_seen_subcommand_from $breakp $linkw $movew $neww $swapw" -xs t -d 'destination window'
complete -c tmux -n "__fish_seen_subcommand_from $killw $lastp $nextl $prevl $renamew" -xs t -d 'target window'
complete -c tmux -n "__fish_seen_subcommand_from $resizew $reswpawnw $rotatew $selectw $unlinkw" -xs t -d 'target window'
complete -c tmux -n "__fish_seen_subcommand_from $displayp" -xs t -d 'target client'
complete -c tmux -n "__fish_seen_subcommand_from $lsp" -xs t -d 'target'

#commands that take shell commands
complete -c tmux -x -n "__fish_seen_subcommand_from $neww $pipep $respawnp $respawnw $splitw" \
          -a '(__fish_complete_subcommand --fcs-skip=2)'

# Common boolean flags. TODO: -P for "print info", -Z for "zoom"
complete -c tmux -n "__fish_seen_subcommand_from $breakp $joinp $linkw $neww $movew $splitw $swapp $swapp" -xs d -d 'do not activate'

set -l updownleftright "$resizep $resizew $selectp "
complete -c tmux -n "__fish_seen_subcommand_from $updownleftright" -xs D -d 'down'
complete -c tmux -n "__fish_seen_subcommand_from $updownleftright" -xs U -d 'up'
complete -c tmux -n "__fish_seen_subcommand_from $updownleftright" -xs L -d 'left'
complete -c tmux -n "__fish_seen_subcommand_from $updownleftright" -xs R -d 'right'

set -l before_after "$breakp $linkw $movew $neww "
complete -c tmux -n "__fish_seen_subcommand_from $before_after" -xs a -d 'after'
complete -c tmux -n "__fish_seen_subcommand_from $before_after" -xs b -d 'before'

# Boolean flags, ct'd. Unclear why these are not -a/-b for after/before
set -l updownnextprev "$rotatew $swapp "
complete -c tmux -n "__fish_seen_subcommand_from $updownnextprev" -xs D -d 'down/next'
complete -c tmux -n "__fish_seen_subcommand_from $updownnextprev" -xs U -d 'up/prev'

# Boolean flags, ct'd. When `-a` does not mean "after"
complete -c tmux -n "__fish_seen_subcommand_from $lsp $lsw" -xs a -d 'all on this server'
complete -c tmux -n "__fish_seen_subcommand_from $killp $killw" -xs a -d 'all except chosen'
complete -c tmux -n "__fish_seen_subcommand_from $prev $next" -xs a -d 'with alert'

###############  End:   Windows and Panes ###############

###############  Begin: Key Bindings ###############
set -l bind 'bind-key bind'
set -l lsk 'list-keys lsk'
set -l send 'send-keys send'
set -l sendprefix send-prefix
set -l unbind 'unbind-key unbind'

set -l key_table 'vi-edit emacs-edit vi-choice emacs-choice vi-copy emacs-copy'

complete -c tmux -n __fish_use_subcommand -a $bind -d 'bind key to command'
complete -c tmux -n "__fish_seen_subcommand_from $bind" -s c -d 'bind for command mode instead of normal mode'
complete -c tmux -n "__fish_seen_subcommand_from $bind" -s n -d 'make the binding work without using a prefix key'
complete -c tmux -n "__fish_seen_subcommand_from $bind" -s r -d 'key may repeat'
complete -c tmux -n "__fish_seen_subcommand_from $bind" -xs t -d 'choose key table for binding' -xa "$key_table"

complete -c tmux -n __fish_use_subcommand -a $lsk -d 'list all key bindings'
complete -c tmux -n "__fish_seen_subcommand_from $lsk" -s t -d 'key table' -xa "$key_table"

complete -c tmux -n __fish_use_subcommand -a $send -d 'list all key bindings'
complete -c tmux -n "__fish_seen_subcommand_from $send" -s R -d 'reset terminal state'
complete -c tmux -n "__fish_seen_subcommand_from $send" -xs t -a '(__fish_tmux_panes)' -d 'target pane'

complete -c tmux -n __fish_use_subcommand -a $sendprefix -d 'send the prefix key'
complete -c tmux -n "__fish_seen_subcommand_from $sendprefix" -s 2 -d 'use secondary prefix'
complete -c tmux -n "__fish_seen_subcommand_from $sendprefix" -xs t -a '(__fish_tmux_panes)' -d 'target pane'

complete -c tmux -n __fish_use_subcommand -a $unbind -d 'unbind the command bound to key'
complete -c tmux -n "__fish_seen_subcommand_from $unbind" -s a -d 'remove all key bindings'
complete -c tmux -n "__fish_seen_subcommand_from $unbind" -s c -d 'binding for command mode'
complete -c tmux -n "__fish_seen_subcommand_from $unbind" -s n -d 'command bound to key without a prefix (if any) removed'
complete -c tmux -n "__fish_seen_subcommand_from $unbind" -xs t -d 'key table' -xa "$key_table"

###############  End:   Key Bindings ###############

###############  Begin: Options ###############
set -l setoption 'set set-option'
set -l showoptions show-options
set -l options \
    backspace \
    buffer-limit \
    command-alias \
    default-terminal \
    copy-command \
    escape-time \
    editor \
    exit-empty \
    exit-unattached \
    extended-keys \
    focus-events \
    history-file \
    message-limit \
    prompt-history-limit \
    set-clipboard \
    terminal-features \
    terminal-overrides \
    user-keys \
    activity-action \
    assume-paste-time \
    base-index \
    bell-action \
    default-command \
    default-shell \
    default-size \
    destroy-unattached \
    detach-on-destroy \
    display-panes-active-colour \
    display-panes-colour \
    display-panes-time \
    display-time \
    history-limit \
    key-table \
    lock-after-time \
    lock-command \
    message-command-style \
    message-style \
    mouse \
    prefix \
    renumber-windows \
    repeat-time \
    set-titles \
    set-titles-string \
    silence-action \
    status \
    status-format \
    status-interval \
    status-justify \
    status-keys \
    status-left \
    status-left-length \
    status-left-style \
    status-position \
    status-right \
    status-right-length \
    status-right-style \
    status-style \
    update-environment \
    visual-activity \
    visual-bell \
    visual-silence \
    word-separators \
    aggressive-resize \
    automatic-rename \
    automatic-rename-format \
    clock-mode-colour \
    clock-mode-style \
    fill-character \
    main-pane-height \
    main-pane-width \
    copy-mode-match-style \
    copy-mode-mark-style \
    copy-mode-current-match-style \
    mode-keys \
    mode-style \
    monitor-activity \
    monitor-bell \
    monitor-silence \
    other-pane-height \
    other-pane-width \
    pane-active-border-style \
    pane-base-index \
    pane-border-format \
    pane-border-indicators \
    pane-border-lines \
    pane-border-status \
    pane-border-style \
    popup-style \
    popup-border-style \
    popup-border-lines \
    window-status-activity-style \
    window-status-bell-style \
    window-status-current-format \
    window-status-current-style \
    window-status-format \
    window-status-last-style \
    window-status-separator \
    window-status-style \
    window-size \
    wrap-search \
    allow-passthrough \
    allow-rename \
    alternate-screen \
    cursor-colour \
    pane-colours \
    cursor-style \
    remain-on-exit \
    remain-on-exit-format \
    scroll-on-clear \
    synchronize-panes \
    window-active-style \
    window-style
complete -c tmux -n __fish_use_subcommand -a "$setoption" -d 'Set or unset option'
complete -c tmux -n __fish_use_subcommand -a "$showoptions" -d 'Show set options'
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions" -s p -d 'Pane option'
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions" -s w -d 'Window option'
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions" -s s -d 'Server option'
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions" -s g -d 'Global option'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s F -d 'Expand format'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s u -d 'Unset option'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s U -d 'Unset option, also in child panes'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s o -d 'Prevent override'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s q -d 'Suppress ambiguous option errors'
complete -c tmux -n "__fish_seen_subcommand_from $setoption" -s a -d Append
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions" -s t -x -d 'Target pane' -a '(__fish_tmux_panes)'
complete -c tmux -n "__fish_seen_subcommand_from $showoptions" -s q -d 'No error if unset'
complete -c tmux -n "__fish_seen_subcommand_from $showoptions" -s v -d 'Only show value'
complete -c tmux -n "__fish_seen_subcommand_from $showoptions" -s H -d 'Include hooks'
complete -c tmux -n "__fish_seen_subcommand_from $showoptions" -s A -d 'Include inherited options'
complete -c tmux -n "__fish_seen_subcommand_from $setoption $showoptions; and not __fish_seen_subcommand_from $options" -a "$options"

###############  End:   Options ###############

###############  Begin: Environment ###############
set -l setenv 'set-environment setenv'
set -l showenv 'show-environment showenv'

complete -c tmux -n __fish_use_subcommand -a $setenv -d 'Set or unset an environment variable'
complete -c tmux -n "__fish_seen_subcommand_from $setenv" -s g -d 'global environment'
complete -c tmux -n "__fish_seen_subcommand_from $setenv" -s r -d 'remove from environment before starting a new process'
complete -c tmux -n "__fish_seen_subcommand_from $setenv" -s u -d 'unset variable'
complete -c tmux -xs t -n "__fish_seen_subcommand_from $setenv" -a '(__fish_tmux_sessions)' -d target-session

complete -c tmux -n __fish_use_subcommand -a $showenv -d 'bind key to command'
complete -c tmux -n "__fish_seen_subcommand_from $showenv" -s g -d 'global environment'
complete -c tmux -xs t -n "__fish_seen_subcommand_from $showenv" -a '(__fish_tmux_sessions)' -d target-session

###############  End:   Environment ###############

###############  Begin: Status Line ###############
set -l commandprompt command-prompt
set -l display 'display-message display'

complete -c tmux -n __fish_use_subcommand -a $commandprompt -d 'Open the command prompt in a client'
complete -c tmux -n "__fish_seen_subcommand_from $commandprompt" -s I -x -d 'Comma-separated list of initial text for each prompt'
complete -c tmux -n "__fish_seen_subcommand_from $commandprompt" -s p -x -d 'Comma-separated list of prompts'
complete -c tmux -n "__fish_seen_subcommand_from $commandprompt" -s t -xa '(__fish_tmux_clients)' -d target-client

complete -c tmux -n __fish_use_subcommand -a $display -d 'Display a message'
complete -c tmux -n "__fish_seen_subcommand_from $display" -s p -d 'print to stdout'
complete -c tmux -n "__fish_seen_subcommand_from $display" -s t -xa '(__fish_tmux_panes)' -d target-pane
complete -c tmux -n "__fish_seen_subcommand_from $display" -s c -xa '(__fish_tmux_clients)' -d target-client

###############  End:   Status Line ###############

###############  Begin: Buffers ###############
#TODO - these commands are not currently implemented.
#there is a section in the tmux man page that has the same title as this section
#use the "Clients and Sessions" code as an example when implementing this
###############  End:   Buffers ###############

###############  Begin: Miscellaneous ###############
#TODO - these commands are not currently implemented.
#there is a section in the tmux man page that has the same title as this section
#use the "Clients and Sessions" code as an example when implementing this
###############  End:   Miscellaneous ###############
