# name: Sorin
# author: Ivan Tham <ivanthamjunhoe@gmail.com>

function fish_prompt
    test $SSH_TTY
    and printf (set_color red)$USER(set_color brwhite)'@'(set_color yellow)(prompt_hostname)' '
    test "$USER" = 'root'
    and echo (set_color red)"#"

    # Main
    echo -n (set_color cyan)(prompt_pwd) (set_color red)'❯'(set_color yellow)'❯'(set_color green)'❯ '
end

function fish_right_prompt
    # last status
    test $status != 0
    and printf (set_color red)"⏎ "

    if git rev-parse 2>/dev/null
        # Magenta if branch detached else green
        set -l branch (command git branch -qv | string match "\**")
        string match -rq detached -- $branch
        and set_color brmagenta
        or set_color brgreen

        git name-rev --name-only HEAD

        # Merging state
        git merge -q 2>/dev/null
        or printf ':'(set_color red)'merge'
        printf ' '

        # Symbols
        if set -l count (command git rev-list --count --left-right $upstream...HEAD 2>/dev/null)
            echo $count | read -l ahead behind
            if test "$ahead" -gt 0
                printf (set_color magenta)⬆' '
            end
            if test "$behind" -gt 0
                printf (set_color magenta)⬇' '
            end
        end

        for i in (git status --porcelain | string sub -l 2 | uniq)
            switch $i
                case "."
                    printf (set_color green)✚' '
                case " D"
                    printf (set_color red)✖' '
                case "*M*"
                    printf (set_color blue)✱' '
                case "*R*"
                    printf (set_color brmagenta)➜' '
                case "*U*"
                    printf (set_color bryellow)═' '
                case "??"
                    printf (set_color brwhite)◼' '
            end
        end
    end
end
