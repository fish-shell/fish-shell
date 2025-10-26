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
        if set -l python (__fish_anypython)
            function __fish_config_webconfig -V python -a web_config
                set -lx __fish_bin_dir $__fish_bin_dir
                $python $web_config/webconfig.py
            end
            __fish_data_with_directory tools/web_config '.*' __fish_config_webconfig
            __fish_with_status functions --erase __fish_config_webconfig

            # If the execution of 'webconfig.py' fails, display python location and return.
            or begin
                echo "Please check if Python has been installed successfully."
                echo "You can find the location of Python by executing the 'command -s $python' command."
                return 1
            end

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

    if not contains -- $cmd prompt theme
        echo No such subcommand: $cmd >&2
        return 1
    end

    # Variables a theme is allowed to set
    set -l theme_var_filter '^fish_(?:pager_)?color.*$'

    switch $cmd
        case prompt
            # prompt - for prompt switching
            set -l cmd $argv[1]
            set -e argv[1]

            if contains -- $cmd list; and set -q argv[1]
                echo "Too many arguments" >&2
                return 1
            end

            switch $cmd
                case show
                    set -l fish (status fish-path)
                    for p in (__fish_config_list_prompts $argv)
                        set -l promptname (string replace -r '.*/([^/]*).fish$' '$1' $p)
                        echo -s (set_color --underline) $promptname (set_color normal)
                        $fish -c '
                            functions -e fish_right_prompt
                            __fish_data_with_file $argv[1] source
                            false
                            fish_prompt
                            echo (set_color normal)
                            if functions -q fish_right_prompt
                                echo right prompt: (false; fish_right_prompt)
                            end' $p
                        echo
                    end
                case list ''
                    __fish_config_list_prompts |
                        string replace -r '.*/([^/]*).fish$' '$1'
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

                    set -l prompt_path (__fish_config_list_prompts $argv[1])
                    if not set -q prompt_path[1]
                        echo "No such prompt: '$argv[1]'" >&2
                        return 1
                    end
                    __fish_config_prompt_reset
                    __fish_data_with_file $prompt_path source
                    if not functions -q fish_mode_prompt
                        __fish_data_with_file functions/fish_mode_prompt.fish source
                    end
                case save
                    if begin
                            read -P"Overwrite prompt? [y/N]" -l yesno
                            not string match -riq 'y(es)?' -- $yesno
                        end
                        echo Not overwriting
                        return 1
                    end
                    echo Overwriting
                    # Skip the cp if unnecessary,
                    # or we'd throw an error on a stock fish.
                    for function in fish_prompt fish_right_prompt fish_mode_prompt
                        path is $__fish_config_dir/functions/$function.fish
                        and cp $__fish_config_dir/functions/$function.fish{,.bak}
                    end

                    if set -q argv[1]
                        set -l prompt_path (__fish_config_list_prompts $argv[1])
                        if not set -q prompt_path[1]
                            echo "No such prompt: '$argv[1]'" >&2
                            return 1
                        end
                        __fish_config_prompt_reset
                        __fish_data_with_file $prompt_path source
                    end

                    funcsave fish_prompt
                    or return

                    for func in fish_right_prompt fish_mode_prompt
                        if functions -q $func
                            funcsave $func
                        else
                            rm -f $__fish_config_dir/functions/$func.fish
                        end
                    end
                    if not functions -q fish_mode_prompt
                        __fish_data_with_file functions/fish_mode_prompt.fish source
                    end
                    return
            end

            return 0
        case theme
            # Selecting themes
            set -l cmd $argv[1]
            set -e argv[1]

            if contains -- $cmd list; and set -q argv[1]
                echo "Too many arguments" >&2
                return 1
            end

            switch $cmd
                case list ''
                    __fish_config_list_theme_names
                    return
                case demo
                    echo -ns (set_color $fish_color_command || set_color $fish_color_normal) /bright/vixens
                    echo -ns (set_color normal) ' '
                    echo -ns (set_color $fish_color_param || set_color $fish_color_normal) jump
                    echo -ns (set_color normal) ' '
                    echo -ns (set_color $fish_color_redirection || set_color $fish_color_normal) '|'
                    echo -ns (set_color normal) ' '
                    echo -ns (set_color $fish_color_quote || set_color $fish_color_normal) '"fowl"'
                    echo -ns (set_color normal) ' '
                    echo -ns (set_color $fish_color_redirection || set_color $fish_color_normal) '> quack'
                    echo -ns (set_color normal) ' '
                    echo -ns (set_color $fish_color_end || set_color $fish_color_normal) '&'
                    set_color normal
                    echo -s (set_color $fish_color_comment || set_color $fish_color_normal) ' # This is a comment'
                    set_color normal
                    echo -ns (set_color $fish_color_command || set_color $fish_color_normal) echo
                    echo -ns (set_color normal) ' '
                    echo -s (set_color $fish_color_error || set_color $fish_color_normal) "'" (set_color $fish_color_quote || set_color $fish_color_normal) "Errors are the portal to discovery"
                    set_color normal
                    echo -ns (set_color $fish_color_command || set_color $fish_color_normal) Th
                    set_color normal
                    set_color $fish_color_autosuggestion || set_color $fish_color_normal
                    echo is an autosuggestion
                    echo
                case show
                    set -l fish (status fish-path)
                    set -l used_themes

                    echo -s (set_color normal; set_color --underline) Current (set_color normal)
                    fish_config theme demo

                    for themename in (__fish_config_list_theme_names $argv)
                        contains -- $themename $used_themes
                        and continue
                        set -a used_themes $themename

                        echo -s (set_color normal; set_color --underline) $themename (set_color normal)

                        # Use a new, --no-config, fish to display the theme.
                        $fish --no-config -c '
                            fish_config theme choose $argv
                            fish_config theme demo $argv
                        ' $themename
                    end

                case choose save
                    if set -q argv[2]
                        echo "Too many arguments" >&2
                        return 1
                    end
                    # The name of the theme to save *from* is optional for `fish_config theme save`
                    if not set -q argv[1] && contains -- $cmd choose
                        echo "Too few arguments" >&2
                        return 1
                    end

                    set -l scope -g

                    if contains -- $cmd save
                        read -P"Overwrite your current theme? [y/N] " -l yesno
                        if not string match -riq 'y(es)?' -- $yesno
                            echo Not overwriting >&2
                            return 1
                        end
                        set scope -U
                    end

                    set -l known_colors (printf %s "\
fish_color_normal
fish_color_command
fish_color_keyword
fish_color_quote
fish_color_redirection
fish_color_end
fish_color_error
fish_color_param
fish_color_option
fish_color_comment
fish_color_selection
fish_color_operator
fish_color_escape
fish_color_autosuggestion
fish_color_cwd
fish_color_user
fish_color_host
fish_color_host_remote
fish_color_cancel
fish_color_search_match
fish_pager_color_progress
fish_pager_color_background
fish_pager_color_prefix
fish_pager_color_completion
fish_pager_color_description
fish_pager_color_selected_background
fish_pager_color_selected_prefix
fish_pager_color_selected_completion
fish_pager_color_selected_description
fish_pager_color_secondary_background
fish_pager_color_secondary_prefix
fish_pager_color_secondary_completion
fish_pager_color_secondary_description
")
                    if not set -q argv[1]
                        # We're persisting whatever current colors are loaded (maybe in the global scope)
                        # to the universal scope, without overriding them from a theme file.
                        # Like above, make sure to erase from other scopes first and ensure known color
                        # variables are defined, even if empty.
                        # This branch is only reachable in the case of `theme save` so $scope is always `-U`.

                        for color in (printf "%s\n" $known_colors (set --names | string match -r $theme_var_filter) | sort -u)
                            if set -q $color
                                # Cache the value from whatever scope currently defines it
                                set -l value $$color
                                set -eg $color
                                set -U $color $value
                            end
                        end
                        return 0
                    end

                    # If we are choosing a theme or saving from a named theme, load the theme now.
                    # Otherwise, we'll persist the currently loaded/themed variables (in case of `theme save`).
                    set -l theme_path (__fish_config_list_themes $argv[1])[1]
                    if not set -q theme_path[1]
                        echo >&2 "No such theme: $argv[1]"
                        echo >&2 Searched (__fish_config_theme_dirs) (
                            if __fish_is_standalone
                                echo "and `status list-files tools/web_config/themes`"
                            end
                        )
                        return 1
                    end

                    set -l defined_colors
                    __fish_data_with_file $theme_path cat | while read -lat toks
                        # The whitelist allows only color variables.
                        # Not the specific list, but something named *like* a color variable.
                        # This also takes care of empty lines and comment lines.
                        string match -rq -- $theme_var_filter $toks[1]
                        or continue
                        # If we're supposed to set universally, remove any shadowing globals
                        # so the change takes effect immediately (and there's no warning).
                        if test x"$scope" = x-U; and set -qg $toks[1]
                            set -eg $toks[1]
                        end
                        set $scope $toks
                        set -a defined_colors $toks[1]
                    end

                    # Set all colors that aren't mentioned to empty
                    for c in $known_colors
                        contains -- $c $defined_colors
                        and continue
                        # Erase conflicting global variables so we don't get a warning and
                        # so changes are observed immediately.
                        set -eg $c
                        set $scope $c
                    end

                    return 0
                case dump
                    # Write the current theme in .theme format, to stdout.
                    set -L | string match -r $theme_var_filter
                case '*'
                    echo "No such command: $cmd" >&2
                    return 1
            end
    end
end

function __fish_config_list_prompts
    set -lx dirs
    if not __fish_is_standalone
        set dirs $__fish_data_dir/tools/web_config/sample_prompts
    end
    set -l prompt_paths (__fish_config_matching tools/web_config/sample_prompts .fish $argv)
    if [ (count $argv) = 1 ] && set -q prompt_paths[2]
        echo >&2 "fish_config: internal error: multiple prompts matching '$argv' ??"
        set --erase prompt_paths[2..]
    end
    string join \n -- $prompt_paths
end

function __fish_config_list_themes
    set -lx dirs (__fish_config_theme_dirs)
    __fish_config_matching tools/web_config/themes .theme $argv
end

function __fish_config_theme_dirs
    printf %s\n $__fish_config_dir/themes
    if not __fish_is_standalone
        printf %s\n $__fish_data_dir/tools/web_config/themes
    end
end

function __fish_config_list_theme_names
    __fish_config_list_themes $argv |
        string replace -r '.*/([^/]*).theme$' '$1'
end

# NOTE: This outputs a mix of absolute and relative paths!
function __fish_config_matching
    set -l prefix $argv[1]
    set -l suffix $argv[2]
    set -e argv[1..2]
    set -l paths
    if not set -q argv[1]
        set paths $dirs/*$suffix
    else
        set paths (path filter $dirs/$argv$suffix)
    end
    if __fish_is_standalone
        if not set -q argv[1]
            set -a paths (status list-files $prefix)
        else
            set -a paths (status list-files $prefix | grep -Fx -e"$prefix/"$argv$suffix)
        end
    end
    string join \n $paths
end

function __fish_config_prompt_reset
    # Set the functions to empty so we empty the file
    # if necessary.
    function fish_prompt
    end
    for func in fish_right_prompt fish_mode_prompt
        if functions -q $func
            functions --erase $func
        end
    end
end
