function __fish_print_help --description "Print help message for the specified fish function or builtin" --argument-names item error_message
    switch $item
        case .
            set item source
        case :
            set item true
        case '['
            set item test
    end

    # Do nothing if the file does not exist
    if not test -e "$__fish_data_dir/man/man1/$item.1" -o -e "$__fish_data_dir/man/man1/$item.1.gz"
        return 2
    end

    # Render help output, save output into the variable 'help'
    set -l help
    set -l format
    set -l cols
    if test -n "$COLUMNS"
        set cols (math $COLUMNS - 4) # leave a bit of space on the right
    end

    # Pick which command we are using to render output or fail if none
    # We prefer mandoc because that doesn't break with unicode input.
    if command -qs mandoc
        set format mandoc -c
        if test -n "$cols"
            set -a format -O width=$cols
        end
    else if command -qs nroff
        set format nroff -c -man -t
        if test -e $__fish_data_dir/groff/fish.tmac
            set -a format -M$__fish_data_dir/groff -mfish
        end
        if test -n "$cols"
            set -a format -rLL={$cols}n
        end
    else
        echo fish: (_ "Cannot format help; no parser found") >&2
        return 1
    end

    if test -e "$__fish_data_dir/man/man1/$item.1"
        # Some nroff versions screw up non-ascii characters.
        # (even with the locale set correctly!)
        # Work around that by running preconv first.
        if command -sq preconv; and test "$format[1]" = nroff
            set help (preconv -e UTF-8 "$__fish_data_dir/man/man1/$item.1" | $format 2>/dev/null)
        else
            set help ($format "$__fish_data_dir/man/man1/$item.1" 2>/dev/null)
        end
    else if test -e "$__fish_data_dir/man/man1/$item.1.gz"
        if command -sq preconv; and test "$format[1]" = nroff
            set help (gunzip -c "$__fish_data_dir/man/man1/$item.1.gz" 2>/dev/null | preconv -e UTF-8 | $format 2>/dev/null)
        else
            set help (gunzip -c "$__fish_data_dir/man/man1/$item.1.gz" 2>/dev/null | $format 2>/dev/null)
        end
    end

    # The original implementation trimmed off the top 5 lines and bottom 3 lines
    # from the nroff output. Perhaps that's reliable, but the magic numbers make
    # me extremely nervous. Instead, let's just strip out any lines that start
    # in the first column. "normal" manpages put all section headers in the first
    # column, but fish manpages only leave NAME like that, which we want to trim
    # away anyway.
    #
    # While we're at it, let's compress sequences of blank lines down to a single
    # blank line, to duplicate the default behavior of `man`, or more accurately,
    # the `-s` flag to `less` that `man` passes.
    set -l state blank
    set -l have_name
    begin
        string join \n $error_message
        for line in $help
            # categorize the line
            set -l line_type
            switch $line
                case ' *' \t\*
                    # starts with whitespace, check if it has non-whitespace
                    printf "%s\n" $line | read -l word __
                    if test -n $word
                        set line_type normal
                    else
                        # lines with just spaces probably shouldn't happen
                        # but let's consider them to be blank
                        set line_type blank
                    end
                case ''
                    set line_type blank
                case '*'
                    # Remove man's bolding
                    set -l name (string replace -ra '(.)'\b'.' '$1' -- $line)
                    # We start after we have the name
                    contains -- $name NAME; and set have_name 1
                    # Everything after COPYRIGHT is useless
                    contains -- $name COPYRIGHT; and break

                    # not leading space, and not empty, so must contain a non-space
                    # in the first column. That makes it a header/footer.
                    set line_type meta
            end

            set -q have_name[1]; or continue
            switch $state
                case normal
                    switch $line_type
                        case normal meta
                            printf "%s\n" $line
                        case blank
                            set state blank
                    end
                case blank
                    switch $line_type
                        case normal meta
                            echo # print the blank line
                            printf "%s\n" $line
                            set state normal
                        case blank meta
                            # skip it
                    end
            end
        end
    end | begin
        set -l pager (__fish_anypager --with-manpager)
        and isatty stdout
        or set pager cat # cannot use a builtin here

        # similar to man, but add -F to quit paging when the help output is brief (#6227)
        # Also set -X for less < v530, see #8157.
        set -l lessopts isRF
        if type -q less; and test (less --version | string match -r 'less (\d+)')[2] -lt 530 2>/dev/null
            set lessopts "$lessopts"X
        end

        not set -qx LESS
        and set -xl LESS $lessopts

        # less options:
        # -i (--ignore-case) search case-insensitively, like man
        # -s (--squeeze-blank-lines) not strictly necessary since we already do that above
        # -R (--RAW-CONTROL-CHARS) to display colors and such
        # -F (--quit-if-one-screen) to maintain the non-paging behavior for small outputs
        # -X (--no-init) do not clear the screen, necessary for less < v530 or else short output is dropped
        $pager
    end
end
