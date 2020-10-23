function prompt_pwd --description "Print the current working directory, shortened to fit the prompt"
    set -l options h/help
    argparse -n prompt_pwd --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help prompt_pwd
        return 0
    end

    # This allows overriding fish_prompt_pwd_dir_length from the outside (global or universal) without leaking it
    set -q fish_prompt_pwd_dir_length
    or set -l fish_prompt_pwd_dir_length 1

    # Replace $HOME with "~"
    set -l realhome ~
    set -l tmp (string replace -r '^'"$realhome"'($|/)' '~$1' $PWD)

    if [ $fish_prompt_pwd_dir_length -eq 0 ]
        echo $tmp
    else
        set -q fish_prompt_dir_full_name_range
        or set -l fish_prompt_dir_full_name_range 1
        if [ $fish_prompt_dir_full_name_range -le 0 ]
            set fish_prompt_dir_full_name_range 1
        end
        set -l folders (string split -rm$fish_prompt_dir_full_name_range / $tmp)
        # Shorten to at most $fish_prompt_pwd_dir_length characters per directory
        echo (string replace -ar '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*/' '$1/' $folders[1]'/')(string join / $folders[2..-1])
    end
end
