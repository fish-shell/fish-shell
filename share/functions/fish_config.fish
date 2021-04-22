function fish_config --description "Launch fish's web based configuration"
    argparse h/help -- $argv
    or return

    if set -q _flag_help
        __fish_print_help fish_config
        return 0
    end

    set -l cmd $argv[1]
    set -e argv[1]

    set -q cmd[1]
    or set cmd browse

    # The web-based configuration UI
    # Also opened with just `fish_config` or `fish_config browse`.
    if contains -- $cmd browse
        set -lx __fish_bin_dir $__fish_bin_dir
        if set -l python (__fish_anypython)
            $python "$__fish_data_dir/tools/web_config/webconfig.py" $argv
        else
            echo (set_color $fish_color_error)Cannot launch the web configuration tool:(set_color normal)
            echo (set_color -o)"fish_config browse"(set_color normal) requires Python.
            echo Installing python will fix this, and also enable completions to be
            echo automatically generated from man pages.\n
            echo To change your prompt, use (set_color -o)"fish_config prompt"(set_color normal) or create a (set_color -o)"fish_prompt"(set_color normal) function.
            echo To list the samples use (set_color -o)"fish_config prompt show"(set_color normal).\n

            echo You can tweak your colors by setting the (set_color $fish_color_search_match)\$fish_color_\*(set_color normal) variables.
        end
        return 0
    end

    if not contains -- $cmd prompt
        echo No such subcommand: $cmd >&2
        return 1
    end
        
    # prompt - for prompt switching
    set -l cmd $argv[1]
    set -e argv[1]

    if contains -- $cmd list; and set -q argv[1]
        echo "Too many arguments" >&2
        return 1
    end

    set -l prompt_dir $__fish_data_dir/sample_prompts $__fish_data_dir/tools/web_config/sample_prompts
    switch $cmd
        case show
            set -l fish (status fish-path)
            set -l prompts $prompt_dir/$argv.fish
            set -q prompts[1]; or set prompts $prompt_dir/*.fish
            for p in $prompts
                if not test -e "$p"
                    continue
                end
                set -l promptname (string replace -r '.*/([^/]*).fish$' '$1' $p)
                echo -s (set_color --underline) $promptname (set_color normal)
                $fish -c "functions -e fish_right_prompt; source $p;
                    false
                    fish_prompt
                    echo (set_color normal)
                    if functions -q fish_right_prompt;
                        echo right prompt: (false; fish_right_prompt)
                    end"
                echo
            end
        case list ''
            string replace -r '.*/([^/]*).fish$' '$1' $prompt_dir/*.fish
            return
        case choose
            if set -q argv[2]
                echo "Too many arguments" >&2
                return 1
            end
            if not set -q argv[1]
                echo "Too few arguments" >&2
                return 1
            end

            set -l have 0
            for f in $prompt_dir/$argv[1].fish
                if test -f $f
                    source $f
                    set have 1
                    break
                end
            end
            if test $have -eq 0
                echo "No such prompt: '$argv[1]'" >&2
                return 1
            end
        case save
            read -P"Overwrite prompt? [y/N]" -l yesno
            if string match -riq 'y(es)?' -- $yesno
                echo Overwriting
                cp $__fish_config_dir/functions/fish_prompt.fish{,.bak}

                if set -q argv[1]
                    set -l have 0
                    for f in $prompt_dir/$argv[1].fish
                        if test -f $f
                            set have 1
                            source $f
                            or return 2
                        end
                    end
                    if test $have -eq 0
                        echo "No such prompt: '$argv[1]'" >&2
                        return 1
                    end
                end

                funcsave fish_prompt
                or return

                functions -q fish_right_prompt
                and funcsave fish_right_prompt

                return
            else
                echo Not overwriting
                return 1
            end
    end

    return 0
end
