# a function to test if prt-get should have ports as potential completions
function __fish_prt_use_port -d 'Test if prt-get should have ports as potential completion'
    for i in (commandline -opc)
        if contains -- $i install depinst grpinst diff depends quickdep dependent deptree isinst info path readme ls cat edit
            return 0
        end
    end
    return 1
end
