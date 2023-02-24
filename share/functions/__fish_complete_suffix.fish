# Find files ending in any of the non-switch arguments and output them as completions.
# * --description provides the description that should be part of each generated completion,
# * --prefix=DIR makes __fish_complete_suffix behave as if it were called in DIR rather than $PWD
# * --complete=PREFIX only lists files that begin with PREFIX
#
# Files matching the above preconditions are printed first then other, non-matching files
# are printed for fallback purposes. As such, it is imperative that `complete` calls that
# shell out to `__fish_complete_suffix` are made with a `-k` switch to ensure sort order
# is preserved.
function __fish_complete_suffix -d "Complete using files"
    set -l _flag_prefix ""
    set -l _flag_complete (commandline -ct)

    argparse 'prefix=' 'description=' 'complete=' -- $argv

    set -l suff (string escape --style=regex -- $argv)

    # Simple and common case: no prefix, just complete normally and sort matching files first.
    if test -z $_flag_prefix
        # Use normal file completions.
        set files (complete -C "__fish_command_without_completions $_flag_complete")
        set -l files_with_suffix (string match -r -- (string join "|" "^.*"$suff\$) $files)
        set -l directories (string match -r -- '^.*/$' $files)
        set files $files_with_suffix $directories $files
    else
        # Only directories are supported as prefixes, and to use the same logic
        # for both absolute prefixed paths and relative non-prefixed paths, $_flag_prefix
        # must terminate in a `/` if it is present, so it can be unconditionally
        # prefixed to any path to get the desired result.
        if not string match -qr '/$' $_flag_prefix
            set _flag_prefix $_flag_prefix/
        end

        # Strip leading ./ as it confuses the detection of base and suffix
        # It is conditionally re-added below.
        set base $_flag_prefix(string replace -r '^("\')?\\./' '' -- $_flag_complete | string trim -c '\'"') # " make emacs syntax highlighting happy

        set -l all
        set -l files_with_suffix
        set -l dirs
        # If $_flag_complete is "./ma" and the file is "main.py", we'll catch that case here,
        # but complete.cpp will not consider it a match, so we have to output the
        # correct form.

        # Also do directory completion, since there might be files with the correct
        # suffix in a subdirectory.
        set all $base*
        set files_with_suffix (string match -r -- (string join "|" ".*"$suff) $all)
        if not string match -qr '/$' -- $argv
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
        if string match -qr '^\\./' -- $_flag_complete
            set files ./$files
        else
            # "Escape" files starting with a literal dash `-` with a `./`
            set files (string replace -r -- "^-" "./-" $files)
        end
    end

    if set -q files[1]
        if string match -qr -- . "$_flag_description"
            set _flag_description "\t$_flag_description"
        end
        if string match -qr -- . "$_flag_prefix"
            set prefix (string escape --style=regex -- $_flag_prefix)
            set files (string replace -r -- "^$prefix" "" $files)
        end
        printf "%s$_flag_description\n" $files
    end

end
