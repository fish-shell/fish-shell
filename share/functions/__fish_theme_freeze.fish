# localization: skip(private)
function __fish_theme_freeze
    set -l data_source $argv[1]
    set -l theme_data $argv[2..]
    set -l relative_path conf.d/fish_frozen_theme.fish
    __fish_backup_config_files $relative_path

    set -l help_section interactive#syntax-highlighting
    status get-file help_sections | string match -q $help_section
    or echo "fish: internal error: missing help section '$help_section'"

    mkdir -p -- (path dirname -- $__fish_config_dir/conf.d)
    printf >$__fish_config_dir/$relative_path %s\n \
        $(test $data_source = __fish_migrate &&
            echo "\
# This file was created by fish when upgrading to version 4.3, to migrate
# theme variables from universal to global scope.") \
        "\
# Don't edit this file, as it will be written by the web-config tool (`fish_config`).
# To customize your theme, delete this file and see
#     help $help_section
# or
#     man fish-interactive | less +/^SYNTAX.HIGHLIGHTING
# for appropriate commands to add to ~/.config/fish/config.fish instead." \
        $(test $data_source = __fish_migrate &&
            echo '# See also the release notes for fish 4.3.0 (run `help relnotes`).') \
        "" \
        'set --global '$theme_data
end
