#
# Find files that complete $argv[1], has the suffix $argv[2], and output them
# as completions with the optional description $argv[3].  Then, also output
# completions for the files that don't have the suffix, so you want to use
# "complete -k" on the output.  Both $argv[1] and $argv[3] are optional,
# if only one is specified, it is assumed to be the argument to complete. If
# $argv[4] is present, it is treated as a prefix for the path, i.e. in lieu
# of $PWD.

function __fish_complete_suffix -d "Complete using files"

    # Variable declarations

    set -l comp
    set -l suff
    set -l desc
    set -l files
    set -l prefix ""

    switch (count $argv)

        case 1
            set comp (commandline -ct)
            set suff $argv
            set desc ""

        case 2
            set comp $argv[1]
            set suff $argv[2]
            set desc ""

        case 3
            set comp $argv[1]
            set suff $argv[2]
            set desc $argv[3]

        case 4
            set comp $argv[1]
            set suff $argv[2]
            set desc $argv[3]
            set prefix $argv[4]

            # Only directories are supported as prefixes, and to use the same logic
            # for both absolute prefixed paths and relative non-prefixed paths, $prefix
            # must terminate in a `/` if it is present, so it can be unconditionally
            # prefixed to any path to get the desired result.
            if not string match -qr '/$' $prefix
                set prefix $prefix/
            end
    end

    # Simple and common case: no prefix, just complete normally and sort matching files first.
    if test -z $prefix
        set -l suffix (string escape --style=regex -- $suff)
        # Use normal file completions.
        set files (complete -C "__fish_command_without_completions $comp")
        set -l files_with_suffix (string match -r -- "^.*$suffix\$" $files)
        set -l directories (string match -r -- '^.*/$' $files)
        set files $files_with_suffix $directories $files
    else
        # Strip leading ./ as it confuses the detection of base and suffix
        # It is conditionally re-added below.
        set base $prefix(string replace -r '^("\')?\\./' '' -- $comp | string trim -c '\'"') # " make emacs syntax highlighting happy

        set -l all
        set -l files_with_suffix
        set -l dirs
        # If $comp is "./ma" and the file is "main.py", we'll catch that case here,
        # but complete.cpp will not consider it a match, so we have to output the
        # correct form.

        # Also do directory completion, since there might be files with the correct
        # suffix in a subdirectory.
        set all $base*
        set files_with_suffix (string match -r -- ".*"(string escape --style=regex -- $suff) $all)
        if not string match -qr '/$' -- $suff
            set dirs $base*/

            # The problem is that we now have each directory included twice in the output,
            # once as `dir` and once as `dir/`. The runtime here is O(n) for n directories
            # in the output, but hopefully since we have only one level (no nested results)
            # it should be fast. The alternative is to shell out to `sort` and remove any
            # duplicate results, but it would have to be a huge `n` to make up for the fork
            # overhead.
            for dir in $dirs
                set all (string match -v (string match -r '(.*)/$' -- $dir)[2] -- $all)
            end
        end

        set files $files_with_suffix $dirs $all
        if string match -qr '^\\./' -- $comp
            set files ./$files
        else
            # "Escape" files starting with a literal dash `-` with a `./`
            set files (string replace -r -- "^-" "./-" $files)
        end
    end

    if set -q files[1]
        if string match -qr -- . "$desc"
            set desc "\t$desc"
        end
        if string match -qr -- . "$prefix"
            set prefix (string escape --style=regex -- $prefix)
            set files (string replace -r -- "^$prefix" "" $files)
        end
        printf "%s$desc\n" $files
    end

end
