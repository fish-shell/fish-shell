# these don't work
#complete vim -a - -d 'Read file from stdin, commands from stderr, which should be a tty'

function __fish_vim_find_tags_path
    set -l max_depth 10
    set -l tags_path tags

    for depth in (seq $max_depth)
        if test -f $tags_path
            echo $tags_path
            return 0
        end

        set tags_path ../$tags_path
    end

    return 1
end

# NB: This function is also used by the nvim completions
function __fish_vim_tags
    set -l token (commandline -ct)
    set -l tags_path (__fish_vim_find_tags_path)
    or return

    # To prevent freezes on a huge tags file (e.g., on one from the Linux
    # kernel source tree), limit matching tag lines to some reasonable amount
    set -l limit 10000

    # tags file is alphabetically sorted, so it's reasonable to use "look" to
    # speedup the lookup of strings with known prefix
    command look $token $tags_path | head -n $limit | cut -f1,2 -d \t
end

# todo
# +[num]        : Position the cursor on line number
# +/{pat}       : Position the cursor on the first occurrence of {pat}
# +{command}    : Execute Ex command after the first file has been read
complete -c vim -s c -r -d 'Execute Ex command after the first file has been read'
complete -c vim -s S -r -d 'Source file after the first file has been read'
complete -c vim -l cmd -r -d 'Execute Ex command before loading any vimrc'
complete -c vim -s d -r -d 'Use device as terminal (Amiga only)'
complete -c vim -s i -r -d 'Set the viminfo file location'
complete -c vim -s o -d 'Open horizontally split windows for each file'
complete -c vim -o o2 -d 'Open two horizontally split windows' # actually -o[N]
complete -c vim -s O -d 'Open vertically split windows for each file'
complete -c nvim -o O2 -d 'Open two vertically split windows' # actually -O[N]
complete -c vim -s p -d 'Open tab pages for each file'
complete -c nvim -o p2 -d 'Open two tab pages' # actually -p[N]
complete -c vim -s q -r -d 'Start in quickFix mode'
complete -c vim -s r -r -d 'Use swap files for recovery'
complete -c vim -s s -r -d 'Source and execute script file'
complete -c vim -s t -xa '(__fish_vim_tags)' -d 'Set the cursor to tag'
complete -c vim -s T -r -d 'Terminal name'
complete -c vim -s u -r -d 'Use alternative vimrc'
complete -c vim -s U -r -d 'Use alternative vimrc in GUI mode'
complete -c vim -s w -r -d 'Record all typed characters'
complete -c vim -s W -r -d 'Record all typed characters (overwrite file)'
complete -c vim -s A -d 'Start in Arabic mode'
complete -c vim -s b -d 'Start in binary mode'
complete -c vim -s C -d 'Behave mostly like vi'
complete -c vim -s d -d 'Start in diff mode'
complete -c vim -s D -d 'Debugging mode'
complete -c vim -s e -d 'Start in Ex mode'
complete -c vim -s E -d 'Start in improved Ex mode'
complete -c vim -s f -d 'Start in foreground mode'
complete -c vim -s F -d 'Start in Farsi mode'
complete -c vim -s g -d 'Start in GUI mode'
complete -c vim -s h -d 'Print help message and exit'
complete -c vim -s H -d 'Start in Hebrew mode'
complete -c vim -s L -d 'List swap files'
complete -c vim -s l -d 'Start in lisp mode'
complete -c vim -s m -d 'Disable file modification'
complete -c vim -s M -d 'Disallow file modification'
complete -c vim -s N -d 'Reset compatibility mode'
complete -c vim -s n -d 'Don\'t use swap files'
complete -c vim -s R -d 'Read only mode'
complete -c vim -s r -d 'List swap files'
complete -c vim -s s -d 'Start in silent mode'
complete -c vim -s V -d 'Start in verbose mode'
complete -c vim -s v -d 'Start in vi mode'
complete -c vim -s x -d 'Use encryption when writing files'
complete -c vim -s X -d 'Don\'t connect to X server'
complete -c vim -s y -d 'Start in easy mode'
complete -c vim -s Z -d 'Start in restricted mode'
complete -c vim -o nb -d 'Become an editor server for NetBeans'
complete -c vim -l no-fork -d 'Start in foreground mode'
complete -c vim -l echo-wid -d 'Echo the Window ID on stdout (GTK GUI only)'
complete -c vim -l help -d 'Print help message and exit'
complete -c vim -l literal -d 'Do not expand wildcards'
complete -c vim -l noplugin -d 'Skip loading plugins'
complete -c vim -l remote -d 'Edit files on Vim server'
complete -c vim -l remote-expr -d 'Evaluate expr on Vim server'
complete -c vim -l remote-send -d 'Send keys to Vim server'
complete -c vim -l remote-silent -d 'Edit files on Vim server'
complete -c vim -l remote-wait -d 'Edit files on Vim server'
complete -c vim -l remote-wait-silent -d 'Edit files on Vim server'
complete -c vim -l serverlist -d 'List all Vim servers that can be found'
complete -c vim -l servername -d 'Set server name'
complete -c vim -l version -d 'Print version information and exit'
complete -c vim -l socketid -r -d 'Run gvim in another window (GTK GUI only)'
complete -c vim -l clean -d 'Factory defaults: skip vimrc, plugins, viminfo'
complete -c vim -l startuptime -r -d 'Write startup timing messages to <file>'
