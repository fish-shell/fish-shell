
complete -x -c cd -a "(__fish_complete_cd)"
complete -c cd -s h -l help --description 'Display help and exit'

function __fish_complete_previous_dir
    set -l dir
    if test "$__fish_cd_direction" = "next"
        set dir $dirnext[-1]
    else
        set dir $dirprev[-1]
    end
    if set -q dir[1]
        printf '%s\t%s\n' - "Previous dir: $dir"
    end
end

complete -c cd -a '(__fish_complete_previous_dir)'
