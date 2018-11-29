# name: Acidhub
# author: Acidhub - https://acidhub.click/

function fish_prompt -d "Write out the prompt"
    set laststatus $status

    if set -l git_branch (command git symbolic-ref HEAD 2>/dev/null | string replace refs/heads/ '')
        set git_branch (set_color -o blue)"$git_branch"
        if command git diff-index --quiet HEAD --
            if set -l count (command git rev-list --count --left-right $upstream...HEAD 2>/dev/null)
                echo $count | read -l ahead behind
                if test "$ahead" -gt 0
                    set git_status "$git_status"(set_color red)⬆
                end
                if test "$behind" -gt 0
                    set git_status "$git_status"(set_color red)⬇
                end
            end
            for i in (git status --porcelain | string sub -l 2 | uniq)
                switch $i
                    case "."
                        set git_status "$git_status"(set_color green)✚
                    case " D"
                        set git_status "$git_status"(set_color red)✖
                    case "*M*"
                        set git_status "$git_status"(set_color green)✱
                    case "*R*"
                        set git_status "$git_status"(set_color purple)➜
                    case "*U*"
                        set git_status "$git_status"(set_color brown)═
                    case "??"
                        set git_status "$git_status"(set_color red)≠
                end
            end
        else
            set git_status (set_color green):
        end
        set git_info "(git$git_status$git_branch"(set_color white)")"
    end

    set_color -b black
    printf '%s%s%s%s%s%s%s%s%s%s%s%s%s' (set_color -o white) '❰' (set_color green) $USER (set_color white) '❙' (set_color yellow) (echo $PWD | sed -e "s|^$HOME|~|") (set_color white) $git_info (set_color white) '❱' (set_color white)
    if test $laststatus -eq 0
        printf "%s✔%s≻%s " (set_color -o green) (set_color white) (set_color normal)
    else
        printf "%s✘%s≻%s " (set_color -o red) (set_color white) (set_color normal)
    end
end
