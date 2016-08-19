function __fish_detect_screen_socket_dir --description "Detect which folder screen uses"
    set screen_bin screen
    if not set -q __fish_screen_socket_dir
        set -g __fish_screen_socket_dir (eval $screen_bin -ls __fish_i_don_t_think_this_will_be_matched | string match -r "(?<=No Sockets found in ).*(?=\.)")
    end
end

function __fish_complete_screen_general_list_mac --description "Get the socket list on mac"
    pushd $__fish_screen_socket_dir > /dev/null
    set -l sockets (ls)
    if test (count $sockets) -ne 0
        switch $argv
            case "Detached"
                stat -f "%Lp %SB %N" -t "%D %T" $sockets | string match -r '^6\d{2} .*$' | string replace -r '^6\d{2} (\S+ \S+) (\S+)' '$2\t$1 Detached'
            case "Attached"
                stat -f "%Lp %SB %N" -t "%D %T" $sockets | string match -r '^7\d{2} .*$' | string replace -r '^7\d{2} (\S+ \S+) (\S+)' '$2\t$1 Attached'
        end
    end
    popd > /dev/null
end

function __fish_complete_screen_general_list --description "Get the socket list"
    screen -list | string match -r '^\t.*\(.*\)\s*\('$argv'\)\s*$'| string replace -r '\t(.*)\s+\((.*)\)\s*\((.*)\)' '$1\t$2 $3'
end

function __fish_complete_screen_detached --description "Print a list of detached screen sessions"
    switch (uname)
        case Darwin
            __fish_complete_screen_general_list_mac Detached
        case '*'
            __fish_complete_screen_general_list Detached
    end
end

function __fish_complete_screen_attached --description "Print a list of attached screen sessions"
    switch (uname)
        case Darwin
            __fish_complete_screen_general_list_mac Attached
        case '*'
            __fish_complete_screen_general_list Attached
    end
end

function __fish_complete_screen --description "Print a list of running screen sessions"
    string join \n (__fish_complete_screen_attached) (__fish_complete_screen_detached)
end


# detect socket directory for mac users
__fish_detect_screen_socket_dir

complete -c screen -x
complete -c screen -s a -d 'Include all capabilitys'
complete -c screen -s A -d 'Adapt window size'
complete -c screen -s c -r -d 'Specify init file'
complete -c screen -s d -d 'Detach screen' -a '(__fish_complete_screen)' -x
complete -c screen -s D -d 'Detach screen' -a '(__fish_complete_screen)' -x
complete -c screen -s r -d 'Reattach session' -a '(__fish_complete_screen)' -x
complete -c screen -s R -d 'Reattach/create session'
complete -c screen -o RR -d 'Reattach/create any session'
complete -c screen -s e -x -d 'Escape character'
complete -c screen -s f -d 'Flow control on'
complete -c screen -o fn -d 'Flow control off'
complete -c screen -o fa -d 'Flow control automatic'
complete -c screen -s h -x -d 'History length'
complete -c screen -s i -d 'Interrupt display on C-c'
complete -c screen -s l -d 'Login on'
complete -c screen -o ln -d 'Login off'
complete -c screen -o ls -d 'List sessions'
complete -c screen -o list -d 'List sessions'
complete -c screen -s L -d 'Log on'
complete -c screen -s m -d 'Ignore $STY'
complete -c screen -s O -d 'Optimal output'
complete -c screen -s p -d 'Preselect window'
complete -c screen -s q -d 'Quiet mode'
complete -c screen -s s -r -d 'Set shell'
complete -c screen -s S -x -d 'Session name'
complete -c screen -s t -x -d 'Session title'
complete -c screen -s U -d 'UTF-8 mode'
complete -c screen -s v -d 'Display version and exit'
complete -c screen -o wipe -d 'Wipe dead sessions'
complete -c screen -s x -d 'Multi attach' -a '(__fish_complete_screen_attached)' -x
complete -c screen -s X -r -d 'Send command'

