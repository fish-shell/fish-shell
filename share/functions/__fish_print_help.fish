function __fish_print_help --description "Print help message for the specified fish function or builtin" --argument item error_message
    if test "$item" = '.'
        set item source
    end

    # Do nothing if the file does not exist
    if not test -e "$__fish_data_dir/man/man1/$item.1" -o -e "$__fish_data_dir/man/man1/$item.1.gz"
        return
    end

    # Render help output, save output into the variable 'help'
    set -l help
    set -l format
    set -l cols
    if test -n "$COLUMNS"
        set cols (math $COLUMNS - 4) # leave a bit of space on the right
    end

    # Pick which command we are using to render output or fail if none
    if command -qs nroff
        set format nroff -c -man -t
        if test -e $__fish_data_dir/groff/fish.tmac
            set -a format -M$__fish_data_dir/groff -mfish
        end
        if test -n "$cols"
            set -a format -rLL={$cols}n
        end
    else if command -qs mandoc
        set format mandoc -c
        if test -n "$cols"
            set -a format -O width=$cols
        end
    else
        echo fish: (_ "Cannot format help; no parser found")
        return 1
    end

    if test -e "$__fish_data_dir/man/man1/$item.1"
        set help ($format "$__fish_data_dir/man/man1/$item.1" 2>/dev/null)
    else if test -e "$__fish_data_dir/man/man1/$item.1.gz"
        set help (gunzip -c "$__fish_data_dir/man/man1/$item.1.gz" 2>/dev/null | $format 2>/dev/null)
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
                    contains -- $name NAME; and set have_name 1; and continue
                    # We ignore the SYNOPSIS header
                    contains -- $name SYNOPSIS; and continue
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
    end | string replace -ra '^       ' '' | ul | # post-process with `ul`, to interpret the old-style grotty escapes
    begin
        set -l pager less
        set -q PAGER
        and echo $PAGER | read -at pager
        not isatty stdout
        and set pager cat # cannot use a builtin here
        # similar to man, but add -F to quit paging when the help output is brief (#6227)
        set -xl LESS isrFX
        # less options:
        # -i (--ignore-case) search case-insensitively, like man
        # -s (--squeeze-blank-lines) not strictly necessary since we already do that above
        # -r (--raw-control-chars) to display bold, underline and colors
        # -F (--quit-if-one-screen) to maintain the non-paging behavior for small outputs
        # -X (--no-init) not sure if this is needed but git uses it
        $pager
    end
end
