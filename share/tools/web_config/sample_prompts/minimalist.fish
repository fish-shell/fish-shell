# name: Minimalist
# author: ridiculous_ghoti

function ghoti_prompt
    set_color $ghoti_color_cwd
    echo -n (path basename $PWD)
    set_color normal
    echo -n ' ) '
end
