function choose --description "Chooses a random item from a list"
    if not set -q argv[1]
        return 2
    end
    echo $argv[(random 1 (count $argv))]
end
