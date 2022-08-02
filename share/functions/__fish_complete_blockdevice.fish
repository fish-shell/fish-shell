# Helper function for completions that need to enumerate block devices.
function __fish_complete_blockdevice
    set -l cmd (commandline -ct)
    test -z "$cmd"
    and set cmd /dev/
    for f in $cmd*
        test -b $f
        and printf "%s\t%s\n" $f "Block device"
        test -d $f
        and printf "%s\n" $f/
    end
end
