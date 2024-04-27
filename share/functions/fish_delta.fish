function fish_delta
    argparse h/help f/no-functions c/no-completions C/no-config d/no-diff n/new V/vendor= -- $argv

    if set -q _flag_help
        __fish_print_help fish_delta
        return 0
    end

    type -q diff
    or set -l _flag_d --no-diff

    set -l vendormode default
    if set -ql _flag_vendor[1]
        if not contains -- $_flag_vendor[-1] default ignore user
            echo "Wrong vendor mode '$_flag_vendor[-1]'." >&2
            echo "Valid values are: default, ignore, new" >&2
            return 2
        end
        set vendormode $_flag_vendor[-1]
    end

    # TODO: Do we want to keep the vendor dirs in here?
    set -l default_function_path $__fish_data_dir/functions
    test "$vendormode" = default && set -a default_function_path $__fish_vendor_functionsdirs

    set -l default_complete_path $__fish_data_dir/completions
    test "$vendormode" = default && set -a default_completions_path $__fish_vendor_completionsdirs

    set -l default_conf_path
    test "$vendormode" = default && set -a default_conf_path $__fish_vendor_confdirs

    set -l user_function_path
    set -l user_complete_path
    set -l user_conf_path $__fish_config_dir/conf.d $__fish_sysconf_dir/conf.d
    test "$vendormode" = user && set -a user_conf_path $__fish_vendor_confdirs

    for dir in $fish_function_path
        if contains -- $vendormode ignore default
            contains -- $dir $__fish_vendor_functionsdirs
            and continue
        end
        contains -- $dir $default_function_path
        or set -a user_function_path $dir
    end
    for dir in $fish_complete_path
        if contains -- $vendormode ignore default
            contains -- $dir $__fish_vendor_completionsdirs
            and continue
        end
        # We don't care about generated completions.
        # They shouldn't be compared at all.
        contains -- $dir $default_complete_path $__fish_cache_dir/generated_completions
        or set -a user_complete_path $dir
    end

    set -l vars
    not set -ql _flag_f[1]
    and set -a vars user_function_path default_function_path
    not set -ql _flag_c[1]
    and set -a vars user_complete_path default_complete_path
    not set -ql _flag_C[1]
    and set -a vars user_conf_path default_conf_path

    if not set -q vars[1]
        echo No directories to check! >&2
        return 2
    end

    set -l have_diff 0

    if isatty stdout
        set -f colors "$(set_color normal)" "$(set_color brblue)" "$(set_color bryellow)" "$(set_color green)" "$(set_color red)"
        set -f pager (__fish_anypager)
        or set pager cat

        if type -q less
            set -l lessopts isRF
            if test (less --version | string match -r 'less (\d+)')[2] -lt 530 2>/dev/null
                set lessopts "$lessopts"X
            end

            not set -qx LESS
            and set -xf LESS $lessopts
        end
    else
        set -f colors "" "" "" "" ""
        set -f pager cat # cannot use a builtin here
    end

    begin
        while set -q vars[1]
            set -l user_var $$vars[1]
            set -l default_var $$vars[2]
            set -l all_changed 0
            # We count config files as "changed" compared to /dev/null
            # because they are being run.
            test "$vars[1]" = user_conf_path
            and set all_changed 1
            set -e vars[..2]
            set -l files (path filter -rf -- $user_var/$argv.fish)
            set -q argv[1]
            or set files (path filter -rf -- $user_var/*)
            set -q files[1]
            and set have_diff 1

            for file in $files
                set -l bn (path basename -- $file)
                set -l def (path filter -rf -- $default_var/$bn)[1]
                or begin
                    if test $all_changed = 0
                        set -ql _flag_n
                        and printf (_ "%sNew%s: %s\n") $colors[2] $colors[1] $file
                        continue
                    else
                        set def /dev/null
                    end
                end

                if type -q diff
                    # We execute diff twice - once to figure out if it's changed,
                    # so we can get nicer output.
                    #
                    if not diff -q -- $file $def >/dev/null 2>&1
                        printf (_ "%sChanged%s: %s\n") $colors[3] $colors[1] $file
                        not set -ql _flag_d[1]
                        and diff -u -- $def $file
                    else
                        printf (_ "%sUnmodified%s: %s\n") $colors[4] $colors[1] $file
                    end
                else
                    # Without diff, we can't really tell if the contents are the same.
                    printf (_ "%sPossibly changed%s: %s\n") $colors[3] $colors[1] $file
                end
            end
        end
        # config.fish is special - it's a single file, not a directory,
        # and it's not precedenced - *both* ~/.config/fish/config.fish and /etc/fish/config.fish
        # are executed.
        if not set -ql _flag_C[1]; and begin
                not set -q argv[1]
                or contains -- config $argv
            end
            for file in (path filter -rf -- $__fish_sysconf_dir/config.fish $__fish_config_dir/config.fish)
                # We count these as "changed" so they show up.
                printf (_ "%sChanged%s: %s\n") $colors[3] $colors[1] $file
                not set -ql _flag_d[1]
                and diff -u -- /dev/null $file
            end
        end
        # Instead of using diff's --color=always, just do the coloring ourselves.
    end | string replace -r -- '^(-.*)$' "$colors[5]\$1$colors[1]" |
        string replace -r -- '^(\+.*)$' "$colors[4]\$1$colors[1]" |
        $pager

    return $have_diff
end
