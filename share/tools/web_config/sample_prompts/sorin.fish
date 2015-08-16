# name: Sorin
# author: Ivan Tham <ivanthamjunhoe@gmail.com>

function fish_prompt
	test $SSH_TTY; and printf (set_color red)(whoami)(set_color white)'@'(set_color yellow)(hostname)' '

    test $USER = 'root'; and echo (set_color red)"#"

    # Main
	echo -n (set_color cyan)(prompt_pwd) (set_color red)'❯'(set_color yellow)'❯'(set_color green)'❯ '
end

function fish_right_prompt
    # last status
    test $status != 0; and printf (set_color red)"⏎ "

	if git rev-parse ^ /dev/null
        # Purple if branch detached else green
        git branch -qv | grep "\*" | grep -q detached
            and set_color purple --bold
            or set_color green --bold

        # Need optimization on this block (eliminate space)
        git name-rev --name-only HEAD

        # Merging state
        git merge -q ^ /dev/null; or printf ':'(set_color red)'merge'
        printf ' '

        # Symbols
		for i in (git branch -qv --no-color|grep \*|cut -d' ' -f4-|cut -d] -f1|tr , \n)\
            (git status --porcelain | cut -c 1-2 | uniq)
			switch $i
                case "*[ahead *"
                    printf (set_color purple)⬆' '
                case "*behind *"
                    printf (set_color purple)⬇' '
				case "."
					printf (set_color green)✚' '
				case " D"
					printf (set_color red)✖' '
				case "*M*"
					printf (set_color blue)✱' '
                case "*R*"
                    printf (set_color purple)➜' '
                case "*U*"
                    printf (set_color brown)═' '
				case "??"
					printf (set_color white)◼' '
			end
		end
	end
end
