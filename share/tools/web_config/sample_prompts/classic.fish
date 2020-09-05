# name: Classic
function fish_prompt --description "Write out the prompt"
    set -l color_cwd
    set -l suffix

    if functions -q fish_is_root_user; and fish_is_root_user
        if set -q fish_color_cwd_root
            set color_cwd $fish_color_cwd_root
        else
            set color_cwd $fish_color_cwd
        end

        set suffix '#'
    else
        set color_cwd $fish_color_cwd
        set suffix '>'
    end

    echo -n -s "$USER" @ (prompt_hostname) ' ' (set_color $color_cwd) (prompt_pwd) (set_color normal) "$suffix "
end
