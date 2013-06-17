# name: Informative GIT
# author: mariuszs - https://github.com/mariuszs

function fish_prompt --description 'Write out the prompt'
  set -l last_status $status

  # PWD
  set_color $fish_color_cwd
  echo -n (prompt_pwd)
  set_color normal

  __informative_git_prompt

  if not test $last_status -eq 0
    set_color $fish_color_error
  end

  echo -n ' $ '
  set_color normal
end

function fish_right_prompt -d "Write out the right prompt"

    set_color -o black
    echo (date +%R)

end
