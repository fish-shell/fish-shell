# name: Screen Savvy
# author: Matthias
function fish_prompt -d "Screen Savvy prompt"
    if test -z "$WINDOW"
        printf '%s%s@%s%s%s%s%s> ' (set_color yellow) $USER (set_color purple) (prompt_hostname) (set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
    else
        printf '%s%s@%s%s%s(%s)%s%s%s> ' (set_color yellow) $USER (set_color purple) (prompt_hostname) (set_color white) (echo $WINDOW) (set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
    end
end
