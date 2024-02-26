# Fish completions for gnome's "dconf" configuration tool

function __fish_dconf_keys
    set -l dir
    set -l key
    # We parse the `dump` output instead of `list`
    # because it allows us to complete non-incrementally
    # i.e. to get the keys directly, without going through
    # `dconf list /`, `dconf list /org/` and so on.
    dconf dump / 2>/dev/null | while read -l line
        if string match -q "[*]" -- $line
            # New directory - just save it for the keys
            set dir /(string trim -c "[]" -- $line)
        else if test -n "$line"
            # New key - output with the dir prepended.
            echo $dir/(string replace -r '=.*' '' -- $line)
        end
    end
end

### Commands
set -l commands read list write reset compile update watch dump load help

complete -f -c dconf
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a read -d 'Read the value of a key'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a list -d 'List the sub-keys and sub-directories of a directory'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a write -d 'Write a new value to a key'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a reset -d 'Delete a key or an entire directory'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a compile -d 'Compile a binary database from keyfiles'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a update -d 'Update the system dconf databases'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a watch -d 'Watch a key or directory for changes'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a dump -d 'Dump an entire subpath to stdout'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a load -d 'Populate a subpath from stdin'
complete -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a help -d 'Display help and exit'

### Arguments to commands
# Technically, reset/watch take a "PATH" (which is a dir or a key)
# while read and write take a KEY, but for now this is close enough.
complete -f -c dconf -n "__fish_seen_subcommand_from read write reset watch" -a "(__fish_dconf_keys)"
# List/dump/load take a dir, which is something that ends in a "/"
complete -f -c dconf -n "__fish_seen_subcommand_from list dump load" -a "(__fish_dconf_keys | string replace -r '/[^/]*\$' '/')"

complete -f -c dconf -n "__fish_seen_subcommand_from help" -a "$commands"
