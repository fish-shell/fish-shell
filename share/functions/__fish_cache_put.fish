# localization: skip(private)
function __fish_cache_put
    set -l cache_file $argv[1]
    touch $cache_file
    set -l dir (path dirname $cache_file)
    chown --reference=$dir $cache_file 2>/dev/null ||
        chown (
            stat --format '%u:%g' $dir 2>/dev/null ||
            stat -f '%u:%g' $dir
        ) $cache_file
end
