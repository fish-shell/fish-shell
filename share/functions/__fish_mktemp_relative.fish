function __fish_mktemp_relative
    # OSX mktemp is rather restricted - no suffix, no way to automatically use TMPDIR
    if not set -q TMPDIR[1]
        set -f TMPDIR /tmp
    end
    # TODO use "argparse --move-unknown"
    set -l mktemp_args
    set -l found_positional false
    for arg in $argv
        switch $arg
            case '-*'
                set -a mktemp_args $arg
            case '*'
                set -a mktemp_args $TMPDIR/$arg.XXXXXX
                set found_positional true
        end
    end
    if not $found_positional
        set -a mktemp_args $TMPDIR/fish.XXXXXX
    end
    mktemp $mktemp_args
end
