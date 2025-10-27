# localization: skip(private)
set -l is_standalone (
    if status get-file config.fish &>/dev/null
        echo true
    else
        echo false
    end
)
function __fish_is_standalone -V is_standalone
    $is_standalone
end
