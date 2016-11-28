function __fish_filter_ant_targets -d "Display targets within an ant build.xml file"
    sed -n "s/^.*<target[^>]* name=[\"']\([^\"']*\)[\"'].*\$/\1/p" <$argv[1]
end
