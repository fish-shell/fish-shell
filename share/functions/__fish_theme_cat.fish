# localization: skip(private)
function __fish_theme_cat -a theme_name
    set -l theme_path (__fish_theme_paths $theme_name)[1]
    if not set -q theme_path[1]
        echo >&2 "No such theme: $theme_name"
        echo >&2 Searched (__fish_theme_dir) "and `status list-files themes`"
        return 1
    end
    __fish_data_with_file $theme_path cat
end
