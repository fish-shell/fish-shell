# localization: skip(private)
function __fish_theme_names
    set -l unique
    for name in (__fish_theme_paths $argv |
            string replace -r '.*/([^/]*).theme$' '$1')
        if not contains -- $name $unique
            set -a unique $name
        end
    end
    string join \n -- $unique
end
