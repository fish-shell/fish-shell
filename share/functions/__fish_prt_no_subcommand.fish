# A function to verify if prt-get (the crux package management tool) needs to be completed by a further command

function __fish_prt_no_subcommand -d 'Test if prt-get has yet to be given the command'
    for i in (commandline -opc)
        if contains -- $i install depinst grpinst update remove sysup lock unlock listlocked diff quickdiff search dsearch fsearch info path readme depends quickdep dependent deptree dup list printf listinst listorphans isinst current ls cat edit help dumpconfig version cache
            return 1
        end
    end
    return 0
end

