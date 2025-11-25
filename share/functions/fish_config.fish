function fish_config --description "Launch fish's web based configuration"
    set -l _flag_color_theme
    set -l _flag_no_override

    argparse h/help color-theme= no-override -- $argv
    or return

    if set -q _flag_help
        __fish_print_help fish_config
        return 0
    end

    set -l cmd $argv[1]
    set -e argv[1]

    set -q cmd[1]
    or set cmd browse

    if set -q _flag_color_theme[1]
        if test $cmd != theme
            or not contains -- "$argv[1]" choose save
            echo >&2 "fish_config: --color-theme: unknown option"
            return 1
        end
        if not contains -- $_flag_color_theme dark light unknown
            echo >&2 "fish_config theme: --color-theme argument must be one of 'dark', 'light' or 'unknown', got: '$_flag_color_theme'"
            return 1
        end
    end
    if set -q _flag_no_override[1]
        if test $cmd != theme
            or test "$argv[1]" != choose
            echo >&2 "fish_config: --no-override: unknown option"
            return 1
        end
    end

    # The web-based configuration UI
    # Also opened with just `fish_config` or `fish_config browse`.
    if test $cmd = browse
        if set -l python (__fish_anypython)
            function __fish_config_webconfig -V python -a web_config
                set -lx __fish_bin_dir $__fish_bin_dir
                set -lx __fish_terminal_color_theme $fish_terminal_color_theme
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
                    __fish_backup_config_files functions/{fish_prompt,fish_right_prompt,fish_mode_prompt}.fish

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
                    __fish_theme_names
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
                    echo -s (set_color normal; set_color --underline) Current (set_color normal)
                    fish_config theme demo
                    __fish_theme_for_each __fish_config_theme_demo $argv
                case choose save
                    __fish_config_theme_choose $cmd $argv --color-theme=$_flag_color_theme $_flag_no_override
                    return
                case dump
                    if set -q argv[1]
                        echo "Too many arguments" >&2
                        return 1
                    end
                    # Write some of the current theme in .theme format, to stdout.
                    for varname in (__fish_theme_variables)
                        __fish_posix_quote $varname $$varname
                        echo
                    end
                case '*'
                    echo "No such command: $cmd" >&2
                    return 1
            end
    end
end

function __fish_config_list_prompts
    set -lx dir
    set -l prompt_paths (__fish_config_files prompts .fish $argv)
    if [ (count $argv) = 1 ] && set -q prompt_paths[2]
        echo >&2 "fish_config: internal error: multiple prompts matching '$argv' ??"
        set --erase prompt_paths[2..]
    end
    string join \n -- $prompt_paths
end

function __fish_config_theme_choose_bad_color_theme -a theme_name desired_color_theme source
    echo >&2 "fish_config theme choose: failed to find '[$desired_color_theme]' section (implied by $source) in '$theme_name' theme"
end

function __fish_config_theme_choose
    set -l cmd $argv[1]
    set -e argv[1]
    set -l _flag_color_theme
    set -l _flag_no_override
    argparse color-theme= no-override -- $argv
    or return
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

    if test $cmd = save
        read -P"Overwrite your current theme? [y/N] " -l yesno
        if not string match -riq 'y(es)?' -- $yesno
            echo Not overwriting >&2
            return 1
        end
        set scope -U
    end

    set -l theme_name $argv[1]
    set -l desired_color_theme $_flag_color_theme

    # Persist the currently loaded/themed variables (in case of `theme save`).
    if not set -q theme_name[1]
        # We're persisting whatever current colors are loaded (maybe in the global scope)
        # to the universal scope, without overriding them from a theme file.
        # Like above, make sure to erase from other scopes first and ensure known color
        # variables are defined, even if empty.
        # This branch is only reachable in the case of `theme save` so $scope is always `-U`.
        for color in (__fish_theme_variables)
            # Cache the value from whatever scope currently defines it
            set -l value $$color
            set -eg $color
            set -U $color $value
        end
        return 0
    end

    function __fish_apply_theme --on-variable fish_terminal_color_theme \
        -V theme_name -V desired_color_theme -V scope
        if set -q __fish_color_theme[1]
            set desired_color_theme $__fish_color_theme
        end

        set -l color_theme
        __fish_config_theme_canonicalize
        if set -q color_theme[1] && not set -q desired_color_theme[1]
            set desired_color_theme $color_theme
        end
        set -l theme_data (__fish_theme_cat $theme_name)
        or return
        set -l override (test -n "$__fish_override" && builtin echo true || builtin echo false)
        set -l theme_is_color_theme_aware false
        set -l color_themes dark light unknown
        for ct in $color_themes
            if contains -- [$ct] $theme_data
                set theme_is_color_theme_aware true
            end
        end

        if $theme_is_color_theme_aware
            if set -q desired_color_theme[1]
                if not contains -- "[$desired_color_theme]" $theme_data
                    __fish_config_theme_choose_bad_color_theme $theme_name "$desired_color_theme" --color-theme=$desired_color_theme
                    return 1
                end
            else
                set desired_color_theme $fish_terminal_color_theme
                if not set -q desired_color_theme[1]
                    echo >&2 "fish_config theme choose: internal error: \$fish_terminal_color_theme not yet initialized"
                    return 1
                end
                if not contains -- "[$desired_color_theme]" $theme_data
                    __fish_config_theme_choose_bad_color_theme $theme_name "$desired_color_theme" \$fish_terminal_color_theme = $desired_color_theme
                    echo >&2 "fish_config theme choose: hint: if your terminal does not report colors, pass --color-theme=light or --color-theme=dark when using color-theme-aware themes"
                    return 1
                end
            end
        else
            if set -q desired_color_theme[1]
                and test "$desired_color_theme" != unknown
                and not contains -- "[$desired_color_theme]" $theme_data
                __fish_config_theme_choose_bad_color_theme $theme_name "$desired_color_theme" --color-theme=$desired_color_theme
                return 1
            end
        end

        set -l color_theme
        string join \n -- $theme_data |
            while read -lat toks
                if $theme_is_color_theme_aware
                    for ct in $color_themes
                        if test "$toks" = [$ct]
                            set color_theme $ct
                            break
                        end
                    end
                    if test "$color_theme" != $desired_color_theme
                        continue
                    end
                end
                set -l varname $toks[1]
                string match -rq -- (__fish_theme_variable_filter) "$varname"
                or continue
                # If we're supposed to set universally, remove any shadowing globals
                # so the change takes effect immediately (and there's no warning).
                if test $scope = -U; and set -qg $varname
                    set -eg $varname
                end
                if $override || not set -q $varname || string match -rq -- '--theme=.*' $$varname
                    set $scope $toks --theme=$theme_name
                end
            end
        if $override
            for c in (__fish_theme_variables)
                string match -rq -- "^--theme=(?!$(string escape --style=regex -- $theme_name)\$).*" $$c
                or continue
                # Erase conflicting global variables so we don't get a warning and
                # so changes are observed immediately.
                set -eg $c
                set $scope $c
            end
        end
    end
    set -l color_theme
    __fish_config_theme_canonicalize
    if set -q desired_color_theme[1] || set -q color_theme[1] || test -n "$fish_terminal_color_theme" || test $cmd = save
        if not set -q _flag_no_override[1]
            __fish_override=true __fish_apply_theme
        end
    end
end

function __fish_config_theme_canonicalize --no-scope-shadowing
    # theme_name
    # color_theme
    if not path is (__fish_theme_dir)/$theme_name.theme
        switch $theme_name
            case 'fish default'
                set theme_name default
            case 'ayu Dark' 'ayu Light' 'ayu Mirage' \
                'Base16 Default Dark' 'Base16 Default Light' 'Base16 Eighties' \
                'Bay Cruise' Dracula Fairground 'Just a Touch' Lava \
                'Mono Lace' 'Mono Smoke' \
                None Nord 'Old School' Seaweed 'Snow Day' \
                'Solarized Dark' 'Solarized Light' \
                'Tomorrow Night Bright' 'Tomorrow Night' Tomorrow
                set theme_name (string lower (string replace -a " " "-" $theme_name))
        end
    end
    switch $theme_name
        case \
            ayu-dark ayu-light \
            base16-default-dark base16-default-light \
            solarized-dark solarized-light
            string match -rq -- '^(?<theme_name>.*)-(?<color_theme>dark|light)$' $theme_name
        case tomorrow
            set color_theme light
        case tomorrow-night
            set theme_name tomorrow
            set color_theme dark
    end
end

function __fish_config_theme_demo
    argparse name= data=+ color-themes=+ -- $argv
    or return
    set -l name $_flag_name
    set -l color_themes $_flag_color_themes
    # Use a new, --no-config, fish to display the theme.
    set -l fish (status fish-path)
    $fish --no-config -c '
        set -l name $argv[1]
        for color_theme in $argv[2..]
            echo -s (set_color normal; set_color --underline) "$name" \
                " ($color_theme color theme)" (set_color normal)
            fish_config theme choose $name --color-theme=$color_theme
            fish_config theme demo
        end
    ' $name $color_themes
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
