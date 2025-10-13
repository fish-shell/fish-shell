# localization: skip(private)
function __fish_data_with_file
    set -l path $argv[1]
    set -l cmd $argv[2..]
    if set -q __fish_data_dir[1]
        if not string match -rq -- ^/ $path
            echo >&2 "error: $(status function) requires absolute paths on non-embedded builds"
            return 1
        end
        $cmd $path
    else
        status get-file $path | $cmd
    end
end
