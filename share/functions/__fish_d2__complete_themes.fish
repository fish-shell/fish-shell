function __fish_d2__complete_themes
    d2 themes |
        sed -n '3,$p' |
        string match --entire --regex '^-' |
        string replace --regex -- '- ([^:]+): (.+)' '$2\t$1'
end
