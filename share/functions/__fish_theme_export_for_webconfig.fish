# localization: skip(private)

set -l quote __fish_posix_quote

function __fish_theme_export_for_webconfig -V quote
    $quote "$(fish_config theme dump)"
    __fish_theme_for_each __fish_theme_export_for_webconfig_one
end

function __fish_theme_export_for_webconfig_one -V quote
    argparse name= data=+ color-themes=+ -- $argv
    or return
    set -l name $_flag_name
    set -l data $_flag_data
    set -l color_themes $_flag_color_themes
    set -l pretty_name (string replace -rf -m1 -- '^#\s*name:\s*(.*)' '$1' $data)
    set -l url (string replace -rf -m1 -- '^#\s*url:\s*(.*)' '$1' $data)
    $quote $name "$pretty_name" "$url" (count $color_themes)
    for color_theme in $color_themes
        $quote $color_theme
        set -l preferred_background
        set -l state out
        for line in $data
            if test $state = out
                if test $line = "[$color_theme]"
                    set state in
                end
            else
                string match -rq -- '^#\s*preferred_background:\s*(?<preferred_background>.*)' $line
                and break
                if string match -rq -- '^\[' $line
                    break
                end
            end
        end
        $quote "$preferred_background"
        fish_config theme choose $name --color-theme=$color_theme
        $quote "$(fish_config theme dump)"
    end
end
