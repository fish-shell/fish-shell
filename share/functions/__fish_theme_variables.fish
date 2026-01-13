# localization: skip(private)
function __fish_theme_variables
    set --names $argv[1] | string match -r (__fish_theme_variable_filter)
    true
end
