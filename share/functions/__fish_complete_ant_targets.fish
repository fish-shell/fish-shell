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
        set -l targets (ant -p -debug -f $buildfile 2> /dev/null | string match -r '^\s\S+.*$' $projecthelp)
        for target in $targets
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

        set -l cache_dir
        if [ \( -n $__fish_user_data_dir \) -a \( -d $__fish_user_data_dir \) ]
            set cache_dir $__fish_user_data_dir/ant_completions
        else
            set cache_dir "$HOME/.local/share/fish/ant_completions"
        end
        mkdir -p $cache_dir

        set -l cache_file $cache_dir/(string escape --style=var $buildfile)
        if [ ! -s $cache_file ]
            # generate cache file if empty
            __parse_ant_targets_from_projecthelp $buildfile > $cache_file
        end

        cat $cache_file
    end

    set -l tokens $argv
    if not set -l buildfile (realpath -eq $buildfile (__get_buildfile $tokens))
        return 1 # return nothing if buildfile does not exist
    end

    __get_ant_targets_from_projecthelp $buildfile
end
