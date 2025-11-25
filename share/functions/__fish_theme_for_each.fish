# localization: skip(private)

function __fish_theme_for_each
    set -l callback $argv[1]
    set -l theme_names $argv[2..]
    for theme_name in (
        set -l matching_names (__fish_theme_names $theme_names)
        if not set -q theme_names[1]
            echo default
            string match -v -- default $matching_names
        else
            string join -- \n $matching_names
        end
    )
        set -l theme_data (__fish_theme_cat $theme_name)
        $callback \
            --name=$theme_name \
            --data=$theme_data
    end
end
