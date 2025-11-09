# localization: skip(private)
# Some of these don't have their own page,
# and adding one would be awkward given that the filename
# isn't guaranteed to be allowed.
# So we override them with the good name.
function __fish_canonicalize_builtin -a cmd
    switch $cmd
        case !
            echo not
        case .
            echo source
        case :
            echo true
        case '['
            echo test
        case '{'
            echo begin
        case '*'
            printf %s $cmd
    end
end
