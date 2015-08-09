
function __fish_commandline_test

    set -l is_function no
    for i in (commandline -poc)
        switch $i
            case -f --f --fu --fun --func --funct --functi --functio --function
                set is_function yes

            case --
                break


        end
    end

    switch $is_function
        case yes
            return 0
    end
    return 1

end
