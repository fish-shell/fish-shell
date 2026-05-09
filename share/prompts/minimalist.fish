# name: Minimalist
# author: ridiculous_fish

function fish_prompt
    set_color $fish_color_cwd
    echo -n (__fish_strip_ctrl (path basename $PWD))
    set_color --reset
    echo -n ' ) '
end
