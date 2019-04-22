function __fish_list_sample_prompts --description 'list all available sample prompts'
    for i in $__fish_sample_prompt_directory/*.fish
        echo (basename $i | cut -d '.' -f 1)
    end
end

complete -f -c use_sample_prompt -a "(__fish_list_sample_prompts)"
