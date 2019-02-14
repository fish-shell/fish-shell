function __fish_complete_ant_targets -d "Print list of targets from build.xml and imported files"
    function __filter_xml_start_tag -d "Filter xml start-tags in a buildfile"
        set -l buildfile $argv[1] # full path to buildfile
        set -l tag_pattern $argv[2] # regex pattern for tagname
        # regex to filter start-tags ignoring newlines and '>' in attr values
        # https://www.debuggex.com/r/wRgxHE1yTIgnjfNz
        string join ' ' <$buildfile | string match -ar "<(?:$tag_pattern)(?:[^>\"']*?(?:(?:'[^']*?')|(?:\"[^\"]*\"))?)*?>"
    end
    function __filter_xml_attr_value -d "Filter xml attr value in a start-tag"
        set -l tag $argv[1] # start-tag
        set -l attr $argv[2] # attr name
        # regex to filter attr values ignoring (single|double) quotes in attr values
        # https://www.debuggex.com/r/x7lhtLJSP4msleik
        string replace -rf "^.*$attr=((?:'(?:.*?)')|(?:\"(?:.*?)\")).*\$" '$1' $tag | string trim -c='"\''
    end
    function __get_buildfile -d "Get a buildfile that will be used by ant"
        set -l tokens $argv # tokens from 'commandline -co'
        set -l prev $tokens[1]
        set -l buildfile "build.xml"
        for token in $argv[2..-1]
            switch $prev
                case -buildfile -file -f
                    set buildfile (eval echo $token)
            end
            set prev $token
        end
        # return last one
        echo $buildfile
    end
    function __parse_ant_targets -d "Parse ant targets in the given build file"
        set -l buildfile $argv[1] # full path to buildfile
        set -l targets (__filter_xml_start_tag $buildfile 'target|extension-point')
        for target in $targets
            set -l target_name (__filter_xml_attr_value $target 'name')
            if [ $status -eq 0 ]
                set -l target_description (__filter_xml_attr_value $target 'description')
                if [ $status -eq 0 ]
                    echo $target_name\t$target_description
                else
                    echo $target_name
                end
            end
        end
    end
    function __get_ant_targets -d "Get ant targets recursively"
        set -l buildfile $argv[1] # full path to buildfile
        __parse_ant_targets $buildfile

        set -l basedir (string split -r -m 1 / $buildfile)[1]
        set -l imports (__filter_xml_start_tag $buildfile 'import')
        for import in $imports
            set -l filepath (__filter_xml_attr_value $import 'file')
            # Set basedir if $filepath is not a full path
            if string match -rvq '^/.*' $filepath
                set filename $basedir/$filepath
            end
            if [ -f $filepath ]
                __get_ant_targets $filepath
            end
        end
    end

    set -l tokens (commandline -co)
    set -l buildfile (realpath -eq $buildfile (__get_buildfile $tokens))
    if [ $status -eq 0 ]
        __get_ant_targets $buildfile
    end
end
