# localization: skip(private)
function __fish_with_file
    set -l file $argv[1]
    set -l cmd $argv[2..]
    if set -q __fish_data_dir[1]
        $cmd $__fish_data_dir/$file
    else
        status get-file $file | $cmd
    end
end
