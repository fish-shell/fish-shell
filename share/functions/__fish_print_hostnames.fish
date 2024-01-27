function __fish_print_hostnames -d "Print a list of known hostnames"
    # This function used to primarily query `getent hosts` and only read from `/etc/hosts` if
    # `getent` did not exist or `getent hosts` failed, based off the (documented) assumption that
    # the former *might* return more hosts than the latter, which has never been officially noted
    # to be the case. As `getent` is several times slower, involves shelling out, and is not
    # available on some platforms (Cygwin and at least some versions of macOS, such as 10.10), that
    # order is now reversed and `getent hosts` is only used if the hosts file is not found at
    # `/etc/hosts` for portability reasons.

    begin
        test -r /etc/hosts && read -z </etc/hosts | string replace -r '#.*$' ''
        or type -q getent && getent hosts 2>/dev/null
    end |
        # Ignore own IP addresses (127.*, 0.0[.0[.0]], ::1), non-host IPs (fe00::*, ff00::*),
        # and leading/trailing whitespace. Split results on whitespace to handle multiple aliases for
        # one IP.
        string replace -irf '^\s*?(?!(?:0\.|127\.|ff0|fe0|::1))\S+\s*(.*?)\s*$' '$1' |
        string split ' '

    # Print nfs servers from /etc/fstab
    if test -r /etc/fstab
        string match -r '^\s*[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}:|^[a-zA-Z\.]*:' </etc/fstab |
            string replace -r ':.*' ''
    end

    # Check hosts known to ssh.
    # Yes, seriously - the default specifies both with and without "2".
    # Termux puts these in the android data directory if not rooted.
    # The directory is available as $PREFIX/etc, but that variable name is so generic that
    # it would cause false-positives.
    # Also, some people might use /usr/local/etc.
    set -l known_hosts ~/.ssh/known_hosts{,2} \
        {/data/data/com.termux/files/usr,/usr/local,}/etc/ssh/{,ssh_}known_hosts{,2}
    # Check default ssh configs.
    set -l ssh_config ~/.ssh/config

    # Inherit settings and parameters from `ssh` aliases, if any
    if functions -q ssh
        # Get alias and commandline options.
        set -l ssh_func_tokens (functions ssh | string match '*command ssh *' | string split ' ')
        set -l ssh_command $ssh_func_tokens (commandline -cpx)
        # Extract ssh config path from last -F short option.
        if contains -- -F $ssh_command
            set -l ssh_config_path_is_next 1
            for token in $ssh_command
                if contains -- -F $token
                    set ssh_config_path_is_next 0
                else if test $ssh_config_path_is_next -eq 0
                    set ssh_config (eval "echo $token")
                    set ssh_config_path_is_next 1
                end
            end
        end
    end

    # Extract ssh config paths from Include option
    function _ssh_include --argument-names ssh_config
        # Relative paths in Include directive use /etc/ssh or ~/.ssh depending on
        # system or user level config. -F will not override this behaviour
        set -l relative_path $HOME/.ssh
        if string match '/etc/ssh/*' -- $ssh_config
            set relative_path /etc/ssh
        end

        function _recursive --no-scope-shadowing
            set -l paths
            for config in $argv
                if test -r "$config" -a -f "$config"
                    set paths $paths (
                    # Keep only Include lines and remove Include syntax
                    string replace -rfi '^\s*Include\s+' '' <$config \
                    # Normalize whitespace
                    | string trim | string replace -r -a '\s+' ' ')
                end
            end

            set -l new_paths
            for path in $paths
                set -l expanded_path
                # Scope "relative" paths in accordance to ssh path resolution
                if string match -qrv '^[~/]' $path
                    set path $relative_path/$path
                end
                # Use `eval` to expand paths (eg ~/.ssh/../test/* to /home/<user>/test/file1 /home/<user>/test/file2),
                # and `set` will prevent "No matches for wildcard" messages
                eval set expanded_path $path
                for path in $expanded_path
                    # Skip unusable paths.
                    test -r "$path" -a -f "$path"
                    or continue
                    echo $path
                    set new_paths $new_paths $path
                end
            end

            if test -n "$new_paths"
                _recursive $new_paths
            end
        end
        _recursive $ssh_config
    end
    set -l ssh_configs /etc/ssh/ssh_config (_ssh_include /etc/ssh/ssh_config) $ssh_config (_ssh_include $ssh_config)

    for file in $ssh_configs
        if test -r $file
            # Don't read from $file twice. We could use `while read` instead, but that is extremely
            # slow.
            read -alz -d \n contents <$file

            # Print hosts from system wide ssh configuration file
            # Multiple names for a single host can be given separated by spaces, so just split it explicitly (#6698).
            string replace -rfi '^\s*Host\s+(\S.*?)\s*$' '$1' -- $contents | string split " " | string match -rv '[\*\?]'
            # Also extract known_host paths.
            set known_hosts $known_hosts (string replace -rfi '.*KnownHostsFile\s*' '' -- $contents)
        end
    end

    # Read all files and operate on their combined content
    for file in $known_hosts
        if test -r $file
            read -z <$file
        end
    end |
        # Ignore hosts that are hashed, commented or @-marked and strip the key
        # Handle multiple comma-separated hostnames sharing a key, too.
        #
        # This one regex does everything we need, finding all matches including comma-separated
        # values, but fish does not let us print only a capturing group without the entire match,
        # and we can't use `string replace` instead (because CSV then fails).
        # string match -ar "(?:^|,)(?![@|*!])\[?([^ ,:\]]+)\]?"
        #
        # Instead, manually piece together the regular expressions
        string match -v -r '^\s*[!*|@#]' | string replace -rf '^\s*(\S+) .*' '$1' |
        string split ',' | string replace -r '\[?([^\]]+).*' '$1'

    return 0
end
