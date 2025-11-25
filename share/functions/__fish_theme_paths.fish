# localization: skip(private)
function __fish_theme_paths
    __fish_config_files --user-dir=(__fish_theme_dir) \
        tools/web_config/themes .theme $argv
end
