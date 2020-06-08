function __fish_anypython
    # Try python3 first, because that's usually faster and generally nicer.
    for py in python3 python3.{9,8,7,6,5,4,3} python2 python2.7 python
        command -sq $py
        and echo $py
        and return 0
    end
    # We have no python.
    return 1
end
