# name: Simple

function ghoti_prompt
    # This is a simple prompt. It looks like
    # alfa@nobby /path/to/dir $
    # with the path shortened and colored
    # and a "#" instead of a "$" when run as root.
    set -l symbol ' $ '
    set -l color $ghoti_color_cwd
    if ghoti_is_root_user
        set symbol ' # '
        set -q ghoti_color_cwd_root
        and set color $ghoti_color_cwd_root
    end

    echo -n $USER@$hostname

    set_color $color
    echo -n (prompt_pwd)
    set_color normal

    echo -n $symbol
end
