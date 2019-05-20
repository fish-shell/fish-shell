function __fish_complete_ant_targets -d "Print list of targets from build.xml and imported files"
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
    function __parse_ant_targets_from_projecthelp -d "Parse ant targets from projecthelp"
        set -l buildfile $argv[1] # full path to buildfile
        set -l targets (ant -p -debug -f $buildfile 2> /dev/null | string match -r '^\s[[:graph:]].*$')
        for target in $targets
            # Use [[:graph:]] and [[:print:]] to ignore ANSI escape code
            set -l tokens (string match -r '^\s([[:graph:]]+)(?:\s+([[:print:]]+))?' "$target")
            if [ (count $tokens) -ge 3 ]
                echo $tokens[2]\t$tokens[3]
            else if [ (count $tokens) -ge 2 ]
                echo $tokens[2]
            end
        end
    end
    function __get_ant_targets_from_projecthelp -d "Get ant targets from projecthelp"
        set -l buildfile $argv[1] # full path to buildfile

        if [ \( -z "$XDG_CACHE_HOME" \) -o \( ! -d "$XDG_CACHE_HOME" \) ]
            set XDG_CACHE_HOME "$HOME/.cache"
        end

        set -l cache_dir "$XDG_CACHE_HOME/fish/ant_completions"
        mkdir -p $cache_dir

        set -l cache_file $cache_dir/(fish_md5 -s $buildfile)
        if [ ! -s "$cache_file" ]
            # generate cache file if empty
            __parse_ant_targets_from_projecthelp $buildfile >$cache_file
        end

        cat $cache_file
    end

    set -l tokens $argv
    set -l buildfile (realpath -eq $buildfile (__get_buildfile $tokens))
    if [ $status -ne 0 ]
        return 1 # return nothing if buildfile does not exist
    end

    __get_ant_targets_from_projecthelp $buildfile
end
