function __fish_termux_api__complete_colors
    set_color --print-colors | string match --invert --regex '^(br.*|normal)$'
end
