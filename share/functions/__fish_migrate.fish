# localization: skip(private)
function __fish_migrate
    functions -e __fish_migrate

    set -l migration_version 4300

    # Maybe migrate.
    if set -q __fish_initialized && test $__fish_initialized -ge $migration_version
        return
    end

    # Create empty configuration directories if they do not already exist
    test -e $__fish_config_dir/completions/ -a -e $__fish_config_dir/conf.d/ -a -e $__fish_config_dir/functions/ ||
        mkdir -p $__fish_config_dir/{completions, conf.d, functions}

    # Create config.fish with some boilerplate if it does not exist
    test -e $__fish_config_dir/config.fish || echo "\
if status is-interactive
# Commands to run in interactive sessions can go here
end" >$__fish_config_dir/config.fish

    set -l mark_migration_done set -U __fish_initialized $migration_version
    if not set -q __fish_initialized
        $mark_migration_done
        return
    end

    set -l removing_uvars false
    set -l msg
    # Copy legacy uvars to globals.
    set -l theme_uvars (__fish_theme_variables --universal)
    if set -q theme_uvars[1]
        set removing_uvars true
        set -l msg_suffix .
        if not __fish_config_theme_uvars_subset_of_historical_default $theme_uvars
            for varname in $theme_uvars
                set -g $varname $$varname
            end
            set -l theme_data
            for varname in $theme_uvars
                set -a theme_data "$(string escape -- $varname $$varname | string join " ")"
            end
            __fish_theme_freeze __fish_migrate $theme_data
            set msg_suffix " by default."\n"  Migrated them to global variables set in $(set_color --underline)$(
                    __fish_unexpand_tilde $__fish_config_dir/conf.d/fish_frozen_theme.fish
                )$(set_color normal)"
        end
        set -a msg "* Color variables are no longer set in universal scope$msg_suffix"
        set -a msg '  To restore syntax highlighting in other fish sessions, please restart them.'
    end
    if set -Uq fish_key_bindings
        set removing_uvars true
        set -l relative_filename conf.d/fish_frozen_key_bindings.fish
        set -l filename $__fish_config_dir/$relative_filename
        __fish_backup_config_files $relative_filename
        mkdir -p -- (path dirname -- $filename)
        echo >$filename "\
# This file was created by fish when upgrading to version 4.3, to migrate
# the 'fish_key_bindings' variable from its old default scope (universal)
# to its new default scope (global).  We recommend you delete this file
# and configure key bindings in ~/.config/fish/config.fish if needed.

$(
    # No need to freeze the default value.
    test "$fish_key_bindings" = fish_default_key_bindings
    # Nor if the user has shadowed our previous uvar.
    or set -qg fish_key_bindings
    and echo "# "
)set --global fish_key_bindings $fish_key_bindings

# Prior to version 4.3, fish shipped an event handler that runs
# `set --universal fish_key_bindings fish_default_key_bindings`
# whenever the fish_key_bindings variable is erased.
# This means that as long as any fish < 4.3 is still running on this system,
# we cannot complete the migration.
# As a workaround, erase the universal variable at every shell startup.
set --erase --universal fish_key_bindings"
        set -a msg '* The fish_key_bindings variable is no longer set in universal scope by default.'
        set -a msg (echo -s "  Migrated it to a global variable set in  " \
            "$(set_color --underline)$(__fish_unexpand_tilde $filename)" \
            (set_color normal))
        source $__fish_config_dir/$relative_filename
    end
    $mark_migration_done
    if $removing_uvars
        echo -s (set_color --bold) 'fish:' (set_color normal) " upgraded to version 4.3:"
        string join \n -- $msg
        echo 'See also the release notes (type `help relnotes`).'
        set -Ue fish_key_bindings $theme_uvars
        set -l sh (__fish_posix_shell)
        eval "$sh -c 'sleep 7 # Please read above notice about universal variables' </dev/null &>/dev/null &"
    end
end

function __fish_config_theme_matches --no-scope-shadowing
    set -l varname $argv[1]
    set -l possible_values $argv[2..]
    if not set -q possible_values[1]
        set possible_values normal ""
    end
    set -a checked_varnames $varname
    not set -q $varname
    or contains -- "$$varname" $possible_values
end
function __fish_config_theme_uvars_subset_of_historical_default
    set -l checked_varnames
    set -l matches __fish_config_theme_matches
    $matches fish_color_keyword "$fish_color_command"
    and $matches fish_color_option "$fish_color_param"
    and $matches fish_color_autosuggestion brblack
    and $matches fish_color_cancel -r
    and $matches fish_color_command normal blue
    and $matches fish_color_comment red
    and $matches fish_color_cwd green
    and $matches fish_color_cwd_root red
    and $matches fish_color_end green
    and $matches fish_color_error brred
    and $matches fish_color_escape brcyan
    and $matches fish_color_history_current --bold
    and $matches fish_color_host
    and $matches fish_color_host_remote yellow
    and $matches fish_color_normal
    and $matches fish_color_operator brcyan
    and $matches fish_color_param cyan
    and $matches fish_color_quote yellow
    and $matches fish_color_redirection "cyan --bold"
    and $matches fish_color_search_match \
        "bryellow --background=brblack" \
        "bryellow --background=brblack --bold" \
        "white --background=brblack" \
        "white --background=brblack --bold"
    and $matches fish_color_selection "white --background=brblack --bold"
    and $matches fish_color_status red
    and $matches fish_color_user brgreen
    and $matches fish_color_valid_path --underline
    and $matches fish_color_background
    and $matches fish_pager_color_background
    and $matches fish_pager_color_completion
    and $matches fish_pager_color_description "yellow -i" "yellow --italics"
    and $matches fish_pager_color_prefix "normal --bold --underline"
    and $matches fish_pager_color_progress \
        "brwhite --background=cyan" \
        "brwhite --background=cyan --bold"
    and $matches fish_pager_color_secondary_background
    and $matches fish_pager_color_secondary_completion
    and $matches fish_pager_color_secondary_description
    and $matches fish_pager_color_secondary_prefix
    and $matches fish_pager_color_selected_background -r
    and $matches fish_pager_color_selected_completion
    and $matches fish_pager_color_selected_description
    and $matches fish_pager_color_selected_prefix
    and for uvar in $argv
        contains $uvar $checked_varnames
        or test -z "$$uvar"
        or return
    end
end
