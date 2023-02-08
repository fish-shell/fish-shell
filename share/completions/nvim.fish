type --quiet __fish_vim_tags || source (status dirname)/vim.fish

# Options shared with vim, copied from vim.fish
complete -c nvim -s c -r -d 'Execute Ex command after the first file has been read'
complete -c nvim -s S -r -d 'Source file after the first file has been read'
complete -c nvim -l cmd -r -d 'Execute Ex command before loading any vimrc'
complete -c nvim -s i -r -d 'Set the shada file location'
complete -c nvim -s o -d 'Open horizontally split windows for each file'
complete -c nvim -o o2 -d 'Open two horizontally split windows' # actually -o[N]
complete -c nvim -s O -d 'Open vertically split windows for each file'
complete -c nvim -o O2 -d 'Open two vertically split windows' # actually -O[N]
complete -c nvim -s p -d 'Open tab pages for each file'
complete -c nvim -o p2 -d 'Open two tab pages' # actually -p[N]
complete -c nvim -s q -r -d 'Start in quickFix mode'
complete -c nvim -s r -r -d 'Use swap files for recovery'
complete -c nvim -s t -xa '(__fish_vim_tags)' -d 'Set the cursor to tag'
complete -c nvim -s u -r -d 'Use alternative vimrc'
complete -c nvim -s w -r -d 'Record all typed characters'
complete -c nvim -s W -r -d 'Record all typed characters (overwrite file)'
complete -c nvim -s A -d 'Start in Arabic mode'
complete -c nvim -s b -d 'Start in binary mode'
complete -c nvim -s d -d 'Start in diff mode'
complete -c nvim -s D -d 'Debugging mode'
complete -c nvim -s e -d 'Start in Ex mode, execute stdin as Ex commands'
complete -c nvim -s E -d 'Start in Ex mode, read stdin as text into buffer 1'
complete -c nvim -s h -d 'Print help message and exit'
complete -c nvim -s H -d 'Start in Hebrew mode'
complete -c nvim -s L -d 'List swap files'
complete -c nvim -s m -d 'Disable file modification'
complete -c nvim -s M -d 'Disable buffer modification'
complete -c nvim -s n -d 'Don\'t use swap files'
complete -c nvim -s R -d 'Read-only mode'
complete -c nvim -s r -d 'List swap files'
complete -c nvim -s V -d 'Start in verbose mode'
complete -c nvim -s h -l help -d 'Print help message and exit'
complete -c nvim -l noplugin -d 'Skip loading plugins'
complete -c nvim -s v -l version -d 'Print version information and exit'
complete -c nvim -l clean -d 'Factory defaults: skip vimrc, plugins, shada'
complete -c nvim -l startuptime -r -d 'Write startup timing messages to <file>'

# Options exclusive to nvim, see https://neovim.io/doc/user/starting.html
complete -c nvim -s l -r -d 'Execute Lua script'
complete -c nvim -s ll -r -d 'Execute Lua script in uninitialized editor'
complete -c nvim -s es -d 'Start in Ex script mode, execute stdin as Ex commands'
complete -c nvim -s Es -d 'Start in Ex script mode, read stdin as text into buffer 1'
complete -c nvim -s s -r -d 'Execute script file as normal-mode input'

# Server and API options
complete -c nvim -l api-info -d 'Write msgpack-encoded API metadata to stdout'
complete -c nvim -l embed -d 'Use stdin/stdout as a msgpack-rpc channel'
complete -c nvim -l headless -d "Don't start a user interface"
complete -c nvim -l listen -r -d 'Serve RPC API from this address (e.g. 127.0.0.1:6000)'
complete -c nvim -l server -r -d 'Specify RPC server to send commands to'

# Client options
complete -c nvim -l remote -d 'Edit files on nvim server specified with --server'
complete -c nvim -l remote-expr -d 'Evaluate expr on nvim server specified with --server'
complete -c nvim -l remote-send -d 'Send keys to nvim server specified with --server'
complete -c nvim -l remote-silent -d 'Edit files on nvim server specified with --server'

# Unimplemented client/server options
# Support for these options is planned, but they are not implemented yet (February 2023).
# nvim currently prints either a helpful error message or a confusing one ("Garbage after option argument: ...")
# Once they are supported, we can add them back in - see https://neovim.io/doc/user/remote.html for their status.
# complete -c nvim -l remote-wait -d 'Edit files on nvim server'
# complete -c nvim -l remote-wait-silent -d 'Edit files on nvim server'
# complete -c nvim -l serverlist -d 'List all nvim servers that can be found'
# complete -c nvim -l servername -d 'Set server name'
