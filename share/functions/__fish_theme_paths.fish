# localization: skip(private)
function __fish_theme_paths
    __fish_config_files --user-dir=(__fish_theme_dir) \
        themes .theme $argv
end
