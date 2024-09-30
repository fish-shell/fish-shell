function __fish_graphviz__print_colors
    begin
        __fish_graphviz__print_x11_colors
        __fish_graphviz__print_svg_colors
    end | sort | uniq
end
