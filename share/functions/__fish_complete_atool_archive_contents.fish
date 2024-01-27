function __fish_complete_atool_archive_contents --description 'List archive contents'
    set -l cmd (commandline -cxp)
    set -e cmd[1]
    set -l file
    for arg in $cmd
        switch $arg
            case '-*'
            case '*'
                if test -f $arg
                    set file $arg
                    break
                end
        end
    end
    if not set -q file[1]
        return
    end
    als $file -qq | sed -r 's/^\s*[0-9]+\s+[0-9-]+\s+[0-9:]+\s+(.*)$/\1/'

end
