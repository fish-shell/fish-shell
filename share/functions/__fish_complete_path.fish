function __fish_complete_path --description "Complete using path"
    set -l target
    set -l description
    switch (count $argv)
        case 0
            # pass
        case 1
            set target "$argv[1]"
        case 2 "*"
            set target "$argv[1]"
            set description "$argv[2]"
    end
    set -l targets (complete -C"'' $target")
    if set -q targets[1]
        printf "%s\n" $targets\t"$description"
    end
end
