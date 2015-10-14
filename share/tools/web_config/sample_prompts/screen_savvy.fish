# name: Screen Savvy
# author: Matthias
function fish_prompt -d "Write out the prompt"
 if test -z $WINDOW
   printf '%s%s@%s%s%s%s%s> ' (set_color yellow) (whoami) (set_color purple) (uname -n) (set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
 else
   printf '%s%s@%s%s%s(%s)%s%s%s> ' (set_color yellow) (whoami) (set_color purple) (uname -n) (set_color white) (echo $WINDOW) (set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
 end
end
