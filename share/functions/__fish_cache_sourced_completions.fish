function __fish_cache_sourced_completions
    # Allow a `--name=foo` option which ends up in the filename.
    argparse -s name= -- $argv
    or return

    set -q argv[1]
    or return 1

    set -l cmd (command -s $argv[1])
    or begin
        # If we have no command, we can't get an mtime
        # and so we can't cache
        # The caller can more easily retry
        return 127
    end

    set -l cachedir (__fish_make_cache_dir completions)
    or return

    set -l stampfile $cachedir/$argv[1].stamp
    set -l compfile $cachedir/$argv[1].fish

    set -l mtime (path mtime -- $cmd)

    set -l cmtime 0
    path is -rf -- $stampfile
    and read cmtime <$stampfile

    # If either the timestamp or the completion file don't exist,
    # or the mtime differs, we rerun.
    #
    # That means we'll rerun if the command was up- or downgraded.
    if path is -vrf -- $stampfile $compfile || test "$cmtime" -ne "$mtime" 2>/dev/null
        $argv >$compfile
        # If the command exited unsuccessfully, we assume it didn't work.
        or return 2
        echo -- $mtime >$stampfile
    end

    if path is -rf -- $compfile
        source $compfile
        return 0
    end
    return 3
end
