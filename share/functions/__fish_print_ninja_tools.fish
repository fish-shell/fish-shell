function __fish_print_ninja_tools
    echo list
    if [ -f build.ninja ]
        ninja -t list | string match -v '*:' | string replace -r '\s+(\w+).*' '$1'
    end
end
