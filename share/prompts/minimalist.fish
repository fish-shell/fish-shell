# name: Minimalist
# author: ridiculous_fish

function fish_prompt
    set_color $fish_color_cwd
    echo -n (path basename $PWD)
    set_color --reset
    echo -n ' ) '
end
