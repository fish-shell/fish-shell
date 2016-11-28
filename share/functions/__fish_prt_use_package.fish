# a function to verify if prt-get should have packages as potential completion
function __fish_prt_use_package -d 'Test if prt-get should have packages as potential completion'
    for i in (commandline -opc)
        if contains -- $i update remove lock unlock current
            return 0
        end
    end
    return 1
end
