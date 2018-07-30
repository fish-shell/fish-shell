#
# Find files that complete $argv[1], has the suffix $argv[2], and
# output them as completions with the optional description $argv[3] Both
# $argv[1] and $argv[3] are optional, if only one is specified, it is
# assumed to be the argument to complete.
#

function __fish_complete_suffix -d "Complete using files"

    # Variable declarations

    set -l comp
    set -l suff
    set -l desc
    set -l files

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

    end

    # Strip leading ./ as it confuses the detection of base and suffix
    # It is conditionally re-added below.
    set base (string replace -r '^("\')?\\./' '' -- $comp | string trim -c '\'"') # " make emacs syntax highlighting happy
    # echo "base: $base" > /dev/tty
    # echo "suffix: $suff" > /dev/tty

    set -l all
    set -l dirs
    # If $comp is "./ma" and the file is "main.py", we'll catch that case here,
    # but complete.cpp will not consider it a match, so we have to output the
    # correct form.

    # Also do directory completion, since there might be files with the correct
    # suffix in a subdirectory. `eval` is used since $suff may be passed in
    # as {.foo,.bar} and we want to expand that.
    eval "set all $base*$suff"
    if not string match -qr '/$' -- $suff
        eval "set dirs $base*/"

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

    set files $all $dirs
    if string match -qr '^\\./' -- $comp
        set files ./$files
    end

    # Another problem is that expanded paths are not matched, either.
    # So an expression like $HOME/foo*.zip will expand to /home/rdahl/foo-bar.zip
    # but that no longer matches the expression at the command line.
    if string match -qr '[${}*~]' -- $comp
        set -l expanded
        eval "set expanded $comp"
        set files (string replace -- $expanded $comp $files)
    end

    if set -q files[1]
        if not string match -q -- "$desc" ""
           set -l desc "\t$desc"
        end
        printf "%s$desc\n" $files #| sort -u
    end

end
