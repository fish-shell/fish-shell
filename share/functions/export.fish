function export --description 'Set global variable. Alias for set -g, made for bash compatibility'
        if test -z "$argv"
           set
           return 0
        end
        for arg in $argv
            set -l v (echo $arg|tr '=' \n)
            set -l c (count $v)
            switch $c
                    case 1
                            set -gx $v $$v
                    case 2
                            set -gx $v[1] $v[2]
            end
        end
end
