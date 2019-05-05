# Generates a list of parent directories for a given path
# i.e. /a/b/c/d -> [/a/b/c/, /a/b/, /a/, and /a]
function __fish_parent_directories
    if test (count $argv) -ne 1
        # for use in completions, so don't spew error messages
        return 1
    end

    set -l splits (string split '/' $argv[1])
    set -l parents

    for split in $splits
        if test (string length "$split") -eq 0
            continue
        end

        set parents "$parents[1]/$split" $parents
    end

    for parent in $parents
        echo $parent
    end

    return 0
end
