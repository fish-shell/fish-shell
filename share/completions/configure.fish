function __fish_parse_configure
    if test (count $argv) -ne 1
        echo "Usage: parse_configure path/to/configure" 1>&2
        return 1
    end

    # `complete` parses `./configure` as `configure` so we have to handle all paths, not just ./
    if not test -x $argv[1]
        printf "Cannot find or execute '%s'\n" $argv[1] 1>&2
        return 1
    end

    # Must support output along the lines of
    #  -h, --help              display this help and exit
    #      --help=short        display options specific to this package
    #      --help=recursive    display the short help of all the included packages
    #  -V, --version           display version information and exit
    #  -q, --quiet, --silent   do not print `checking ...' messages

    set -l next_line
    set -l line
    set -l buffer
    # Just fish's `./configure --help` takes ~350ms to run, before parsing
    # The following chain attempts to extract the help message:
    cat $argv[1] | tr \n \u0e | sed -n 's/.*Report the --help message\(.*\?\)ac_status.*/\1/; s/ac_status.*//p' | tr \u0e \n |
        while test -n "$next_line" || read -lL next_line
            # In autoconfigure scripts, the first column wraps at 26 chars
            # echo next_line: $next_line
            # echo old_line: $line
            if test -z "$line"
                set line $next_line
                set next_line "" # mark it as consumed
                continue
            else if string match -qr '^(   |\t){2,}[^-]\S*' -- $next_line
                # echo "continuation line found. Old value of line: " \"$line\"
                set line "$line "(string trim $next_line)
                set next_line "" # mark it as consumed
                continue
            end

            # echo line: $line

            # Search for one or more strings starting with `-` separated by commas
            if string replace -fr '^\s+(-.*?)\s+([^\s\-].*)' '$1\n$2' -- $line | read -lL opts description
                for opt in (string split -n , -- $opts | string trim)

                    if string match -qr -- '--[a-z_0-9-]+=\[.*\]' $opt
                        # --option=[OPTIONAL_VALUE]
                        string replace -r -- '(--[a-z_0-9-]+)=.*' '$1' $opt | read opt
                    else if string match -qr -- '--[a-z_0-9-]+\[=.*\]' $opt
                        # --option[=OPTIONAL_VALUE]
                        string replace -r -- '(--[a-z_0-9-]+)\[=.*' '$1' $opt | read opt
                    else if string match -qr -- '--[a-z_0-9-]+=[A-Z]+' $opt
                        # --option=CLASS_OF_VALUE (eg FILE or DIR)
                        string replace -r -- '(--[a-z_0-9-]+)=.*' '$1' $opt | read opt
                    else if string match -qr -- '--[a-z_0-9-]+=\S+' $opt
                        # --option=literal_value, leave as-is
                    else if string match -qr -- '--[a-z_0-9-]+$' $opt
                        # long option, leave as-is
                    else if string match -qr -- '-[^-]$' $opt
                        # short option, leave as-is
                    else
                        continue
                    end

                    echo "$opt"\t"$description" # parsed by `complete` as value and description
                end
            end

            set line ""
        end
end

complete -c configure -s h -l help -x -a "short recursive" -d "Display help and exit"
complete -c configure -s V -l version -d "Display version and exit"
complete -c configure -s q -l quiet -d "Quiet mode"
complete -c configure -l cache-file -f -d "Cache test results in specified file"
complete -c configure -s C -l config-cache -d "Cache test results in file config.cache"
complete -c configure -s n -l no-create -d "Do not create output files"
complete -c configure -l srcdir -d "Set source directory" -a "(__fish_complete_directories)" -x
complete -c configure -l prefix -d "Architecture-independent install directory" -a "(__fish_complete_directories)" -x
complete -c configure -l exec-prefix -d "Architecture-dependent install directory" -a "(__fish_complete_directories)" -x
complete -c configure -l build -d "Configure for building on BUILD" -x
complete -c configure -l host -d "Cross-compile to build programs to run on HOST" -x
complete -c configure -l target -d "Configure for building compilers for TARGET" -x

# use autoconf's --help to generate completions:
complete -c configure -a '(for tok in (commandline -xpc)
    if string match -q "*configure" -- $tok
        __fish_parse_configure $tok
        break
    end
end)'
