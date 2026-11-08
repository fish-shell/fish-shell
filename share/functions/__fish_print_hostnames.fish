# localization: skip(private)
function __fish_print_hostnames -d "Print a list of known hostnames"
    # This function used to primarily query `getent hosts` and only read from `/etc/hosts` if
    # `getent` did not exist or `getent hosts` failed, based off the (documented) assumption that
    # the former *might* return more hosts than the latter, which has never been officially noted
    # to be the case. As `getent` is several times slower, involves shelling out, and is not
    # available on some platforms (Cygwin and at least some versions of macOS, such as 10.10), that
    # order is now reversed and `getent hosts` is only used if the hosts file is not found at
    # `/etc/hosts` for portability reasons.

    __fish_print_hostnames_from_hosts (
        test -r /etc/hosts && read -z </etc/hosts | string replace -r '#.*$' ''
        or type -q getent && getent hosts 2>/dev/null
    )

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
        # Get alias and commandline options, expanded without running command
        # substitutions, which eval would run on every <Tab>.
        set -l ssh_func_tokens
        for line in (functions ssh | string match '*command ssh *')
            set -a ssh_func_tokens (commandline --input $line -x)
        end
        set -l ssh_command $ssh_func_tokens (commandline -cpx)
        # Extract ssh config path from last -F short option.
        if contains -- -F $ssh_command
            set -l ssh_config_path_is_next 1
            for token in $ssh_command
                if contains -- -F $token
                    set ssh_config_path_is_next 0
                else if test $ssh_config_path_is_next -eq 0
                    set ssh_config $token
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
            set -l new_paths
            for path in (__fish_print_hostnames_ssh_config_args include $argv)
                # Scope "relative" paths in accordance to ssh path resolution
                set path (__fish_print_hostnames_ssh_path $path $relative_path)
                or continue
                for path in (__fish_print_hostnames_ssh_glob $path)
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
            set known_hosts $known_hosts (string replace -rfi '.*KnownHostsFile\s*' '' -- $contents | string split " ")
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

function __fish_print_hostnames_from_hosts -d "Extract hostnames from hosts file entries"
    # Ignore own IP addresses (127.*, 0.0[.0[.0]], ::1), broadcast addresses
    # (255.255.255.255), non-host IPs (fe00::*, ff00::*), and leading/trailing
    # whitespace.
    string replace -irf -- '^\s*?(?!(?:0\.|127\.|ff0|fe0|255\.255\.255\.255|::1))\S+\s*(.*?)\s*$' '$1' $argv |
        # Split results on whitespace to handle multiple aliases for one IP.
        string split ' '
end

function __fish_print_hostnames_ssh_config_args -a keyword -d "Print the arguments of ssh_config lines with a keyword"
    # One per line, from the files after the keyword, a regex matched
    # case-insensitively. Since OpenSSH 8.7, ssh splits a line's arguments
    # with argv_split(): on unquoted spaces and tabs, with '...' and "..."
    # quoting, also within an argument, \ escaping a quote, a backslash or,
    # unquoted, a space, and an argument starting with an unquoted # starting a
    # comment. ssh rejects a line with an unterminated quote.
    set -l lines
    for file in $argv[2..]
        test -r "$file" -a -f "$file"
        and set -a lines (string replace -rfi -- '^\s*(?:'$keyword')(?:\s*=\s*|\s+)(.*?)\s*$' '$1' <$file)
    end

    for line in $lines
        # Most lines quote and escape nothing.
        if not string match -qr -- '["\'\\\\]' $line
            string replace -r -- '(?:^|\s)#.*' '' $line | string match -ar -- '[^ \t]+'
            continue
        end

        set -l chars (string split -- '' $line)
        set -l args
        set -l arg ''
        set -l in_arg 0
        set -l quote ''
        set -l i 0
        while test $i -lt (count $chars)
            set i (math $i + 1)
            set -l c $chars[$i]
            if test $in_arg = 0
                contains -- $c ' ' \t
                and continue
                test $c = '#'
                and break
                set in_arg 1
            end
            set -l next ''
            test $i -lt (count $chars)
            and set next $chars[(math $i + 1)]
            if test $c = '\\'; and begin
                    contains -- $next \" \' \\
                    or test -z "$quote" -a "$next" = ' '
                end
                set arg "$arg$next"
                set i (math $i + 1)
            else if test -z "$quote"; and contains -- $c ' ' \t
                set -a args $arg
                set arg ''
                set in_arg 0
            else if test -z "$quote"; and contains -- $c \" \'
                set quote $c
            else if test $c = "$quote"
                set quote ''
            else
                set arg "$arg$c"
            end
        end
        test -z "$quote"
        or continue
        test $in_arg = 1
        and set -a args $arg
        set -q args[1]
        and printf '%s\n' $args
    end
end

function __fish_print_hostnames_ssh_path -a path relative_dir -d "Expand an ssh_config path"
    # ${NAME} becomes the environment variable, and ssh ignores a path with an
    # unset one. With relative_dir, a relative path is in it. ~ and ~/ become
    # $HOME; ~user is left as is.
    set -l expanded ''
    for piece in (string match -ar -- '\$\{[^}]*\}|\$|[^$]+' $path)
        if set -l name (string match -rg -- '^\$\{(.*)\}$' $piece)
            string match -qr -- '^\w+$' $name
            and set -qx $name
            or return 1
            set piece "$$name"
        end
        set expanded "$expanded$piece"
    end
    if test -n "$relative_dir"; and string match -qrv -- '^[~/]' $expanded
        set expanded $relative_dir/$expanded
    end
    if string match -qr -- '^~(/|$)' $expanded
        set expanded "$HOME"(string sub -s 2 -- $expanded)
    end
    printf '%s\n' $expanded
end

function __fish_print_hostnames_ssh_glob -a pattern -d "Print the paths matching an ssh_config glob"
    # glob(3)'s * and ?, matched one path component at a time with a regex,
    # without eval. Like glob(3), a leading . is matched only explicitly.
    string match -q -- '/*' $pattern
    or return
    set -l paths ''
    for part in (string split -n -- / $pattern)
        set -l next
        for dir in $paths
            if string match -qr -- '[*?]' $part
                set -l regex '^'(string escape --style=regex -- $part |
                    string replace -a '\\*' '.*' | string replace -a '\\?' .)'$'
                set -l entries $dir/*
                string match -q -- '.*' $part
                and set -a entries $dir/.*
                for entry in $entries
                    string match -qr -- $regex (path basename -- $entry)
                    and set -a next $entry
                end
            else
                set -a next $dir/$part
            end
        end
        set paths $next
    end
    set -q paths[1]
    and printf '%s\n' $paths
end
