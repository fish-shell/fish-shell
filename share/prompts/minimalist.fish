# name: Minimalist
# author: ridiculous_fish

function fish_prompt
    set_color $fish_color_cwd
    echo -n (prompt_pwd | path basename)
    set_color --reset
    echo -n ' ) '
end
