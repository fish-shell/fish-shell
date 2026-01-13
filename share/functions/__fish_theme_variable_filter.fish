# localization: skip(private)
# Variables a theme is allowed to set
function __fish_theme_variable_filter
    # The whitelist allows only color variables.
    # Not the specific list, but something named *like* a color variable.
    # This also takes care of empty lines and comment lines.
    #
    # Documented at doc_src/cmds/fish_config.rst
    echo '^fish_(?:pager_)?color_.*$'
end
