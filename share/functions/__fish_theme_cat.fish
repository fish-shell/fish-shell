# localization: skip(private)
function __fish_theme_cat -a theme_name
    set -l theme_path (__fish_theme_paths $theme_name)[1]
    if not set -q theme_path[1]
        echo >&2 "No such theme: $theme_name"
        echo >&2 Searched (__fish_theme_dir) "and `status list-files themes`"
        return 1
    end
    set -l theme_data (if string match -q '/*' -- $theme_path; cat $theme_path; else status get-file $theme_path; end)
    set -l allowed_lines \
        '\s*' \
        '\s*#.*' \
        '\[(dark|light|unknown)\]' \
        (__fish_theme_variable_filter)
    set allowed_lines "^($(string join -- '|' $allowed_lines))\$"
    printf '%s\n' $theme_data | string match -rvq -- $allowed_lines
    and for line in $theme_data
        string match -rq -- $allowed_lines $theme_data
        or printf >&2 "error: unsupported line in theme '%s': %s\n" $theme_name $line
    end
    string join \n $theme_data
    true
end
