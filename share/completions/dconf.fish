# Fish completions for gnome's "dconf" configuration tool

function __fish_dconf_keys
    set -l dir
    set -l key
    # We parse the `dump` output instead of `list`
    # because it allows us to complete non-incrementally
    # i.e. to get the keys directly, without going through
    # `dconf list /`, `dconf list /org/` and so on.
    dconf dump / ^/dev/null | while read -l line
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

complete -f -e -c dconf
# TODO: Descriptions for these commands
complete -A -f -c dconf -n "not __fish_seen_subcommand_from $commands" -a "$commands"

### Arguments to commands
# Technically, reset/watch take a "PATH" (which is a dir or a key)
# while read and write take a KEY, but for now this is close enough.
complete -f -c dconf -n "__fish_seen_subcommand_from read write reset watch" -a "(__fish_dconf_keys)"
# List/dump/load take a dir, which is something that ends in a "/"
complete -f -c dconf -n "__fish_seen_subcommand_from list dump load" -a "(__fish_dconf_keys | string replace -r '/[^/]*\$' '/')"

complete -f -c dconf -n "__fish_seen_subcommand_from help" -a "$commands"
