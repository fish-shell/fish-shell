# localization: skip(private)
function __fish_data_with_directory
    set -l relative_directory $argv[1]
    set -l file_regex $argv[2]
    set -l cmd $argv[3..]
    set -l temp
    set -l directory_ref
    if test $relative_directory = man/man1 && not __fish_tried_to_embed_manpages
        set directory_ref $__fish_man_dir/man1
    else
        set temp (__fish_mktemp_relative -d fish-data)
        or return
        if set -l paths (status list-files $relative_directory |
            string match -r \
                "^$(string escape --style=regex $relative_directory)/(?:$file_regex)\$")
            mkdir -p -- $temp/(string join \n $paths | path dirname | path sort -u)
            or return
            for path in $paths
                status get-file $path >$temp/$path
                or return
            end
        end
        set directory_ref $temp/$relative_directory
    end
    $cmd $directory_ref
    set -l saved_status $status
    if set -q temp[1]
        command rm -r $temp
    end
    return $saved_status
end
