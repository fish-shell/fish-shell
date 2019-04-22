set __fish_sample_prompt_directory $__fish_data_dir/tools/web_config/sample_prompts

function use_sample_prompt --argument-names sample_prompt --description 'Replace current prompt with a given sample prompt'
    if not set -q argv[1]
        printf (_ "%ls: No sample prompt given\n") use_sample_prompt >&2
        return 1
    end
    set -l sample_prompt_file $__fish_sample_prompt_directory/$sample_prompt.fish
    if not test -r $sample_prompt_file
        printf (_ "%ls: Unkown sample prompt given\n") use_sample_prompt >&2
        return 1
    end
    set -l local_prompt_file $__fish_config_dir/functions/fish_prompt.fish
    cp $sample_prompt_file $local_prompt_file
    source $local_prompt_file
end
