# localization: skip(private)
function __fish_anypython
    if set -l path (command -s python3)
        # Do not consider the stub /usr/bin/python3 that comes installed on Darwin to be Python
        # unless Xcode reports a real directory path.
        and not begin
            string match -q Darwin -- "$(uname)"
            and string match -q /usr/bin/python3 -- $path
            and type -q xcode-select
            and not xcode-select --print-path &>/dev/null
        end
        echo $path
        return 0
    end
    set -l best_path
    set -l best_minor_version
    set -l minor_version
    for path in (path filter -fx -- $PATH/python3.*)
        string match -rq -- '.*/python3\.(?<minor_version>\d+)$' $path
        and test $minor_version -ge 5 # Python 3.5+
        or continue
        and begin
            not set -q best_path[1]
            or test $minor_version -gt $best_minor_version
        end
        set best_path $path
        set best_minor_version $minor_version
    end
    if set -q best_path[1]
        echo $best_path
        return 0
    end
    # We have no python.
    return 1
end
