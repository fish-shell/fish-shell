
function lst --description "Return last value of provided list."
    set -q argv[1]; and echo $argv[-1]
end
