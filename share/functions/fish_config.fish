function fish_config --description "Launch fish's web based configuration"
    set -lx __fish_bin_dir $__fish_bin_dir
    if set -l python (__fish_anypython)
        $python "$__fish_data_dir/tools/web_config/webconfig.py" $argv
    else
        echo (set_color $fish_color_error)Cannot launch the web configuration tool:(set_color normal)
        echo (set_color -o)fish_config(set_color normal) requires Python.
        echo Installing python2 or python3 will fix this, and also enable completions to be
        echo automatically generated from man pages.\n
        echo To change your prompt, create a (set_color -o)fish_prompt(set_color normal) function.
        echo There are examples in (set_color $fish_color_valid_path)$__fish_data_dir/tools/web_config/sample_prompts(set_color normal).\n
        echo You can tweak your colors by setting the (set_color $fish_color_search_match)\$fish_color_\*(set_color normal) variables.
    end
end
