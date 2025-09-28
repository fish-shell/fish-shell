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
        set -l fish_path (status fish-path)
        and set __fish_bin_dir (path dirname -- $fish_path)
        if set -l python (__fish_anypython)
            set -l mainfile $__fish_data_dir/tools/web_config/webconfig.py
            set -l temp
            if not path is -- $mainfile
                if not status list-files tools/web_config &>/dev/null
                    echo "Cannot find web configuration tool. Please check your fish installation."
                    return 1
                end
                set temp (__fish_mktemp_relative -d fish_config)
                or return
                for dir in (status list-files tools/web_config |
                            path dirname | path sort -u)
                    mkdir -p $temp/$dir
                    or return
                end
                for file in (status list-files tools/web_config)
                    status get-file $file >$temp/$file
                    or return
                end
                set mainfile $temp/tools/web_config/webconfig.py
            end

            $python "$mainfile" $argv
            set -l saved_status $status

            if set -q temp[1]
                command rm -r $temp
            end

            # If the execution of 'webconfig.py' fails, display python location and return.
            if test $saved_status -ne 0
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

            set -l prompt_dir $__fish_data_dir/tools/web_config/sample_prompts
            switch $cmd
                case show
                    set -l fish (status fish-path)
                    set -l prompts (__fish_config_matching tools/web_config/sample_prompts .fish $argv)
                    for p in $prompts
                        set -l promptname (string replace -r '.*/([^/]*).fish$' '$1' $p)
                        echo -s (set_color --underline) $promptname (set_color normal)
                        $fish -c 'functions -e fish_right_prompt;
                        if string match -q "tools/*" -- $argv[1]
                            status get-file $argv[1] | source
                        else
                            source $argv[1]
                        end
                        false
                        fish_prompt
                        echo (set_color normal)
                        if functions -q fish_right_prompt;
                        echo right prompt: (false; fish_right_prompt)
                    end' $p
                        echo
                    end
                case list ''
                    files=$prompt_dir/*.theme string replace -r '.*/([^/]*).fish$' '$1' \
                        $files (status list-files tools/web_config/sample_prompts/ 2>/dev/null)
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

                    set -l have
                    for f in $prompt_dir/$argv[1].fish
                        if test -f $f
                            source $f
                            set have $f
                            break
                        end
                    end
                    if not set -q have[1]
                        if status list-files tools/web_config/sample_prompts/$argv[1].fish &>/dev/null
                            status get-file tools/web_config/sample_prompts/$argv[1].fish | source
                            # HACK: `source` gives us a filename of "-", so we check manually if we had a right prompt
                            set have ""
                            status get-file tools/web_config/sample_prompts/$argv[1].fish | string match -q '*function fish_right_prompt*'
                            and set have -
                        else
                            echo "No such prompt: '$argv[1]'" >&2
                            return 1
                        end
                    end

                    # Erase the right prompt if it didn't have any.
                    if functions -q fish_right_prompt; and test (functions --details fish_right_prompt) != $have[1]
                        functions --erase fish_right_prompt
                    end
                case save
                    read -P"Overwrite prompt? [y/N]" -l yesno
                    if string match -riq 'y(es)?' -- $yesno
                        echo Overwriting
                        # Skip the cp if unnecessary,
                        # or we'd throw an error on a stock fish.
                        path is $__fish_config_dir/functions/fish_prompt.fish
                        and cp $__fish_config_dir/functions/fish_prompt.fish{,.bak}
                        path is $__fish_config_dir/functions/fish_right_prompt.fish
                        and cp $__fish_config_dir/functions/fish_right_prompt.fish{,.bak}

                        set -l have
                        if set -q argv[1]
                            for f in $prompt_dir/$argv[1].fish
                                if test -f $f
                                    set have $f
                                    # Set the functions to empty so we empty the file
                                    # if necessary.
                                    function fish_prompt
                                    end
                                    function fish_right_prompt
                                    end
                                    source $f
                                    or return 2
                                end
                            end
                            if not set -q have[1]
                                if status list-files tools/web_config/sample_prompts/$argv[1].fish &>/dev/null
                                    status get-file tools/web_config/sample_prompts/$argv[1].fish | source
                                else
                                    echo "No such prompt: '$argv[1]'" >&2
                                    return 1
                                end
                            end
                        end

                        funcsave fish_prompt
                        or return

                        funcsave fish_right_prompt 2>/dev/null
                        return
                    else
                        echo Not overwriting
                        return 1
                    end
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

            set -l dirs $__fish_config_dir/themes $__fish_data_dir/tools/web_config/themes

            switch $cmd
                case list ''
                    files=$dirs/*.theme string replace -r '.*/([^/]*).theme$' '$1' \
                        $files (status list-files tools/web_config/themes/ 2>/dev/null)
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
                    set -l themes \
                        (path filter $dirs/$argv.theme) \
                        (__fish_config_matching tools/web_config/themes .theme $argv)
                    set -l used_themes

                    echo -s (set_color normal; set_color --underline) Current (set_color normal)
                    fish_config theme demo

                    for t in $themes
                        set -l themename (string replace -r '.*/([^/]*).theme$' '$1' $t)
                        contains -- $themename $used_themes
                        and continue
                        set -a used_themes $themename

                        echo -s (set_color normal; set_color --underline) $themename (set_color normal)

                        # Use a new, --no-config, fish to display the theme.
                        # So we can use this function, explicitly source it before anything else!
                        functions fish_config | $fish -C "source -" --no-config -c '
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
                    set -l have_colors

                    if contains -- $cmd save
                        read -P"Overwrite your current theme? [y/N] " -l yesno
                        if not string match -riq 'y(es)?' -- $yesno
                            echo Not overwriting >&2
                            return 1
                        end
                        set scope -U
                    end

                    set -l known_colors fish_color_{normal,command,keyword,quote,redirection,\
                        end,error,param,option,comment,selection,operator,escape,autosuggestion,\
                        cwd,user,host,host_remote,cancel,search_match} \
                        fish_pager_color_{progress,background,prefix,completion,description,\
                        selected_background,selected_prefix,selected_completion,selected_description,\
                        secondary_background,secondary_prefix,secondary_completion,secondary_description}

                    # If we are choosing a theme or saving from a named theme, load the theme now.
                    # Otherwise, we'll persist the currently loaded/themed variables (in case of `theme save`).
                    if set -q argv[1]
                        set -l files $dirs/$argv[1].theme
                        set -l file

                        for f in $files
                            if test -e "$f"
                                set file $f
                                break
                            end
                        end

                        if not set -q file[1]
                            if status list-files tools/web_config/themes/$argv[1].theme &>/dev/null
                                set file tools/web_config/themes/$argv[1].theme
                            else
                                echo "No such theme: $argv[1]" >&2
                                echo "Searched directories: $dirs" >&2
                                return 1
                            end
                        end

                        set -l content
                        if string match -qr '^tools/' -- $file
                            set content (status get-file $file)
                        else
                            read -z content <$file
                        end

                        printf %s\n $content | while read -lat toks
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
                            set -a have_colors $toks[1]
                        end

                        # Set all colors that aren't mentioned to empty
                        for c in $known_colors
                            contains -- $c $have_colors
                            and continue

                            # Erase conflicting global variables so we don't get a warning and
                            # so changes are observed immediately.
                            set -eg $c
                            set $scope $c
                        end
                    else
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
                    end

                    # If we've made it this far, we've either found a theme file or persisted the current
                    # state (if any). In all cases we haven't failed, so return 0.
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

function __fish_config_matching
    set -l prefix $argv[1]
    set -l suffix $argv[2]
    set -e argv[1..2]
    set -l paths
    if set -q __fish_data_dir[1]
        if not set -q argv[1]
            set paths $__fish_data_dir/$prefix/*$suffix
        else
            set paths (path filter $__fish_data_dir/$prefix/$argv$suffix)
        end
    else
        if not set -q argv[1]
            set paths (status list-files $prefix)
        else
            set paths (status list-files $prefix | grep -Fx -e"$prefix/"$argv$suffix)
        end
    end
    string join \n $paths
end
