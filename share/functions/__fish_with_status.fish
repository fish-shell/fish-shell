# localization: skip(private)
function __fish_with_status
    set -l saved_status $status
    $argv
    or return
    return $saved_status
end
