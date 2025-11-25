# localization: skip(private)
function __fish_theme_cat -a theme_name
    set -l theme_path (__fish_theme_paths $theme_name)[1]
    if not set -q theme_path[1]
        echo >&2 "No such theme: $theme_name"
        echo >&2 Searched (__fish_theme_dir) "and `status list-files themes`"
        return 1
    end
    set -l theme_data (__fish_data_with_file $theme_path cat)
    set -l allowed_lines \
        '\s*' \
        '\s*#.*' \
        '\[(dark|light|unknown)\]' \
        (__fish_theme_variable_filter)
    set allowed_lines "^($(string join -- '|' $allowed_lines))\$"
    for line in $theme_data
        string match -rq -- $allowed_lines $line
        or printf >&2 "error: unsupported line in theme '%s': %s\n" $theme_name $line
    end
    string join \n $theme_data
    true
end
