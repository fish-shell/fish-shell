function __fish_anypython
    # Try python3 first, because that's usually faster and generally nicer.
    # Do not consider the stub /usr/bin/python3 that comes installed on Darwin to be Python
    # unless Xcode reports a real directory path.
    for py in python3 python3.{12,11,10,9,8,7,6,5,4,3} python2 python2.7 python
        if set -l py_path (command -s $py)
            if string match -q /usr/bin/python3 -- $py_path
                and string match -q Darwin -- "$(uname)"
                and type -q xcode-select
                and not xcode-select --print-path &>/dev/null
                continue
            end
            echo $py
            return 0
        end
    end
    # We have no python.
    return 1
end
