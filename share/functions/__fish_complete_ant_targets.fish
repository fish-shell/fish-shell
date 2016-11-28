function __fish_complete_ant_targets -d "Print list of targets from build.xml and imported files"
    set -l buildfile "build.xml"
    if test -f $buildfile
        # show ant targets
        __fish_filter_ant_targets $buildfile

        # find files with buildfile
        set files (sed -n "s/^.*<import[^>]* file=[\"']\([^\"']*\)[\"'].*\$/\1/p" < $buildfile)

        # iterate through files and display their targets
        for file in $files

            __fish_filter_ant_targets $file
        end
    end
end
