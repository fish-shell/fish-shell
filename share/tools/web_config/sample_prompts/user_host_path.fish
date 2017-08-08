# name: User, Host, Path
# author: Jon Clayden

function fish_prompt -d "Write out the prompt"
    set -l home_escaped (echo -n $HOME | sed 's/\//\\\\\//g')
    set -l pwd (echo -n $PWD | sed "s/^$home_escaped/~/" | sed 's/ /%20/g')
    set -l prompt_symbol ''
    switch "$USER"
        case root toor
            set prompt_symbol '#'
        case '*'
            set prompt_symbol '$'
    end
    printf "[%s@%s %s%s%s]%s " $USER (prompt_hostname) (set_color $fish_color_cwd) $pwd (set_color normal) $prompt_symbol
end
