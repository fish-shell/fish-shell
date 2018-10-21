function __fish_print_hostnames -d "Print a list of known hostnames"
    # Print all hosts from /etc/hosts. Use 'getent hosts' on OSes that support it
    # (OpenBSD and Cygwin do not).
    #
    # Test if 'getent hosts' works and redirect output so errors don't print.
    if type -q getent
        and getent hosts >/dev/null 2>&1
        # Ignore zero IPs.
        getent hosts | string match -r -v '^0.0.0.0' | string replace -r '^\s*\S+\s+' '' | string split ' '
    else if test -r /etc/hosts
        # Ignore commented lines and functionally empty lines.
        string match -r -v '^\s*0.0.0.0|^\s*#|^\s*$' </etc/hosts | string replace -r -a '#.*$' '' | string replace -r '^\s*\S+\s+' '' | string trim | string replace -r -a '\s+' ' ' | string split ' '
    end

    # Print nfs servers from /etc/fstab
    if test -r /etc/fstab
        string match -r '^\s*[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3]:|^[a-zA-Z\.]*:' </etc/fstab | string replace -r ':.*' ''
    end

    # Check hosts known to ssh.
    # Yes, seriously - the default specifies both with and without "2".
    # Termux puts these in the android data directory if not rooted.
    # The directory is available as $PREFIX/etc, but that variable name is so generic that
    # it would cause false-positives.
    # Also, some people might use /usr/local/etc.
    set -l known_hosts ~/.ssh/known_hosts{,2} {/data/data/com.termux/files/usr,/usr/local,}/etc/ssh/{,ssh_}known_hosts{,2}
    # Check default ssh configs.
    set -l ssh_config
    # Get alias and commandline options.
    set -l ssh_func_tokens (functions ssh | string match '*command ssh *' | string split ' ')
    set -l ssh_command $ssh_func_tokens (commandline -cpo)
    # Extract ssh config path from last -F short option.
    if contains -- '-F' $ssh_command
        set -l ssh_config_path_is_next 1
        for token in $ssh_command
            if contains -- '-F' $token
                set ssh_config_path_is_next 0
            else if test $ssh_config_path_is_next -eq 0
                set ssh_config (eval "echo $token")
                set ssh_config_path_is_next 1
            end
        end
    else
        set ssh_config $ssh_config ~/.ssh/config
    end

    # Extract ssh config paths from Include option
    function _ssh_include --argument-names ssh_config
        # Relative paths in Include directive use /etc/ssh or ~/.ssh depending on
        # system or user level config. -F will not override this behaviour
        set -l relative_path $HOME/.ssh
        if string match '/etc/ssh/*' -- $ssh_config
            set relative_path '/etc/ssh'
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
            # Print hosts from system wide ssh configuration file
            string replace -rfi '^\s*Host\s+' '' <$file | string trim | string replace -r '\s+' ' ' | string split ' ' | string match -v '*\**'
            # Extract known_host paths.
            set known_hosts $known_hosts (string replace -rfi '.*KnownHostsFile\s*' '' <$file)
        end
    end
	for file in $known_hosts
        if test -r $file
          # Ignore hosts that are hashed, commented or @-marked and strip the key.
          awk '$1 !~ /[|#@]/ {
            n=split($1, entries, ",")
            for (i=1; i<=n; i++) {
              # Ignore negated/wildcarded hosts.
              if (!match(entry=entries[i], "[!*?]")) {
                # Extract hosts with custom port.
                if (substr(entry, 1, 1) == "[") {
                  if (pos=match(entry, "]:.*$")) {
                    entry=substr(entry, 2, pos-2)
                  }
                }
                print entry
              }
            }
          }' $file
        end
    end
    return 0
end
