function __fish_print_ninja_targets
    if [ -f build.ninja ]
        ninja -t targets 2>/dev/null | string replace -r ':.*' ''
    end
end
