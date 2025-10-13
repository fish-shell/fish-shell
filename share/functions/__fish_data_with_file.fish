# localization: skip(private)
function __fish_data_with_file
    set -l path $argv[1]
    set -l cmd $argv[2..]
    if set -q __fish_data_dir[1]
        if not string match -rq -- ^/ $path
            set path $__fish_data_dir/$path
        end
        $cmd $path
    else
        status get-file $path | $cmd
    end
end
