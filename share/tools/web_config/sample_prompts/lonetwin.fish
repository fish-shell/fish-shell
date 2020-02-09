# name: lonetwin
# author: Steve

function fish_prompt --description 'Write out the prompt'
    echo -n -s "$USER" @ (prompt_hostname) ' ' (set_color $fish_color_cwd) (prompt_pwd) (fish_vcs_prompt) (set_color normal) '> '
end
