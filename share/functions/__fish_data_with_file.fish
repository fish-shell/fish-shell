# localization: skip(private)
function __fish_data_with_file
    set -l path $argv[1]
    set -l cmd $argv[2..]
    if string match -rq -- ^/ $path
        $cmd $path
    else
        status get-file $path | $cmd
    end
end
