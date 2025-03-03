function __fish_wine_explorer__complete_desktop_arg
    set -l current_token (commandline -tc)

    switch "$current_token"
        case '/desktop=*x*'
            return
        case '/desktop=*'
            printf '%sx' "$current_token"
    end
end

function __fish_msiexec__option_completion_condition
    set -l token (commandline -oc)[3]

    not string match --quiet --regex '^/(uninstall|[ipyz])$|^/[fjql]' -- "$token"
end

function __fish_msiexec_complete_option_arg
    set -l current_token (commandline -tc)

    switch "$current_token"
        case '/f*'
            set values 'p\tReinstall the file if it is missing' \
                'o\tReinstall the file if it is missing or if any older version is installed' \
                'e\tReinstall the file if it is missing, or if the installed version is equal or older' \
                'd\tReinstall the file if it is missing or a different version is installed' \
                'c\tReinstall the file if it is missing or the checksum does not match' \
                'a\tReinstall all files' \
                'u\tRewrite all required user registry entries' \
                'm\tRewrite all required machine registry entries' \
                's\tOverwrite any conflicting shortcuts' \
                'v\tRecache the local installation package from the source installation package'

        case '/q*'
            set values 'n\tDisable UI' \
                'b\tShow the basic UI' \
                'r\tShow the reduced UI' \
                'f\tShow the full UI'

        case '/l*'
            set values '*\tEnable all options except v and x' \
                'i\tEnable status messages' \
                'w\tEnable warning messages' \
                'e\tEnable error messages' \
                'a\tEnable messages for action startups' \
                'r\tEnable messages for action records' \
                'u\tEnable messages for user requests' \
                'c\tEnable messages for initial UI parameters' \
                'm\tEnable messages for out of memory errors' \
                'o\tEnable messages for out of disk space errors' \
                'p\tEnable messages for terminal properties' \
                'v\tEnable verbose messages' \
                'x\tEnable messages for debugging' \
                '+\tAppend messages to a file' \
                '!\tFlush each line of messages'
    end

    for value in $values
        echo -e "$current_token$value"
    end
end

function __fish_wine__complete_cmd_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from cmd'
    complete -c $command -a '(__fish_cmd__complete_args)' -n $condition

    set -l c_and_k_condition 'not __fish_seen_argument -w c -w k'

    complete -c $command -a /c \
        -d 'Run the command and stop' \
        -n "$condition && $c_and_k_condition"

    complete -c $command -a /k \
        -d 'Run the command and continue' \
        -n "$condition && $c_and_k_condition"

    complete -c $command -a /s \
        -d 'Modify the treatment of string after /c or /k' \
        -n $condition

    complete -c $command -a /q -d 'Turn echo off' -n $condition

    complete -c $command -a /d \
        -d 'Disable execution of AutoRun commands' \
        -n $condition

    set -l a_and_u_condition 'not __fish_seen_argument -w a -w u'

    complete -c $command -a /a \
        -d 'Format internal command output as ANSI' \
        -n "$condition && $a_and_u_condition"

    complete -c $command -a /u \
        -d 'Format internal command output as Unicode' \
        -n "$condition && $a_and_u_condition"

    complete -c $command -a /t \
        -d 'Set the background and foreground color' \
        -n $condition

    complete -c $command -a /e -d 'Manage command extensions' -n $condition

    complete -c $command -a /f \
        -d 'Manage file and directory name completion' \
        -n $condition

    complete -c $command -a /v \
        -d 'Manage delayed environment variable expansion' \
        -n $condition

    complete -c $command -a '/?' -d 'Show help' -n $condition
end

function __fish_wine__complete_control_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from control'

    complete -c $command \
        -a 'COLOR DATE/TIME DESKTOP INTERNATIONAL KEYBOARD MOUSE PORTS PRINTERS' \
        -n $condition
end

function __fish_wine__complete_eject_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from eject'
    complete -c $command -s h -d 'Show help' -n $condition
    complete -c $command -s a -d 'Eject all the CD drives' -n $condition
    complete -c $command -s u -d 'Unmount the CD drives' -n $condition
end

function __fish_wine__complete_explorer_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from explorer'

    complete -c $command -a '(__fish_wine_explorer__complete_desktop_arg)' \
        -n $condition

    complete -c $command -a /n -d 'Use the single pain view' -n $condition
    complete -c $command -a /e, -d 'Use the default view' -n $condition

    complete -c $command -a /root, -d 'Specify the root level of a view' \
        -n $condition

    complete -c $command -a /select, -d 'Specify the selection in a view' \
        -n $condition

    complete -c $command -a /desktop= -d 'Specify the desktop name' \
        -n $condition
end

function __fish_wine__complete_msiexec_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from msiexec'
    complete -c $command -a '/? /h' -d 'Show help' -n $condition

    complete -c $command -a '(__fish_msiexec_complete_option_arg)' \
        -n $condition

    complete -c $command -a /i \
        -d 'Install the software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    set -l a_condition '__fish_seen_argument -w i -w p'

    complete -c $command -a /a \
        -d 'Use the administrator network' \
        -n "$condition && $a_condition"

    complete -c $command -a /f \
        -d 'Repair the installation of software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /uninstall \
        -d 'Uninstall the software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /j \
        -d 'Advertise the software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /p \
        -d 'Apply the patch to software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /q \
        -d 'Change the UI while installing software' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /l \
        -d 'Change the logging' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /y \
        -d 'Register the MSI service' \
        -n "$condition && __fish_msiexec__option_completion_condition"

    complete -c $command -a /z \
        -d 'Register the MSI service' \
        -n "$condition && __fish_msiexec__option_completion_condition"
end

function __fish_wine__complete_regedit_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from regedit'
    complete -c $command -a '/?' -d 'Show help' -n $condition

    complete -c $command -a /L \
        -d 'Specify the location of system.dat file' \
        -n $condition

    complete -c $command -a /R \
        -d 'Specify the location of user.dat file' \
        -n $condition

    complete -c $command -a /C \
        -d 'Import contents of a registry file' \
        -n $condition

    complete -c $command -a /D \
        -d 'Delete a registry key' \
        -n $condition

    complete -c $command -a /E \
        -d 'Export contents to a registry file' \
        -n $condition

    complete -c $command -a /S \
        -d 'Do not display messages' \
        -n $condition

    complete -c $command -a /V \
        -d 'Launch the GUI in an advanced mode' \
        -n $condition

    complete -c $command -a '(__fish_reg__complete_keys)' \
        -n $condition
end

function __fish_wine__complete_start_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from start'
    complete -c $command -a '/?' -d 'Show help' -n $condition

    complete -c $command -a /d \
        -d 'Specify the directory for a program' \
        -n $condition

    complete -c $command -a /b \
        -d "Don't create the new console for a progra" \
        -n $condition

    complete -c $command -a /i \
        -d 'Clear the environment for a program' \
        -n $condition

    complete -c $command -a /min \
        -d 'Start a program in the minimized window' \
        -n $condition

    complete -c $command -a /max \
        -d 'Start a program in the minimized window' \
        -n $condition

    complete -c $command -a /low \
        -d 'Start a program in the idle priority class' \
        -n $condition

    complete -c $command -a /normal \
        -d 'Start a program in the normal priority class' \
        -n $condition

    complete -c $command -a /high \
        -d 'Start a program in the high priority class' \
        -n $condition

    complete -c $command -a /realtime \
        -d 'Start a program in the realtime priority class' \
        -n $condition

    complete -c $command -a /abovenormal \
        -d 'Start a program in the abovenormal priority class' \
        -n $condition

    complete -c $command -a /belownormal \
        -d 'Start a program in the belownormal priority class' \
        -n $condition

    complete -c $command -a /node \
        -d 'Specify the NUMA node for a program' \
        -n $condition

    complete -c $command -a /affinity \
        -d 'Specify the affinity mask for a program' \
        -n $condition

    complete -c $command -a /wait \
        -d 'Wait for a program to exit' \
        -n $condition

    complete -c $command -a /unix \
        -d 'Use the Unix filename for a program' \
        -n $condition
end

function __fish_wine__complete_winemenubuilder_subcommand --argument-names command
    complete -c $command -a /w \
        -d 'Wait till the shortcut can be created' \
        -n '__fish_seen_subcommand_from winemenubuilder'
end

function __fish_wine__complete_winepath_subcommand --argument-names command
    set -l condition '__fish_seen_subcommand_from winepath'
    complete -c $command -s h -d 'Show help' -n $condition
    complete -c $command -s v -d 'Show version' -n $condition

    complete -c $command -s u -l unix \
        -d 'Convert a Windows path to the Unix one' \
        -n $condition

    complete -c $command -s w -l windows \
        -d 'Convert a Unix path to the Windows one' \
        -n $condition

    complete -c $command -s l -l long \
        -d 'Convert a Windows path to the long format' \
        -n $condition

    complete -c $command -s s -l short \
        -d 'Convert a Windows path to the short format' \
        -n $condition
end

set -l command wine

complete -c $command -l help -d 'Show help'
complete -c $command -l version -d 'Show version'

set -l subcommands_with_descriptions 'cacls\t"Edit ACLs"' \
    'clock\t"Open the clock"' \
    'cmd\t"Open the command prompt"' \
    'cmdlgtst\t"commdlg.dll test jig"' \
    'control\t"Open control panel"' \
    'eject\t"Eject optical discs"' \
    'expand\t"Expand cabinet files"' \
    'explorer\t"Open Explorer"' \
    'hh\t"Open HTML help"' \
    'icinfo\t"List installed video compressors"' \
    'iexplore\t"Open Internet Explorer"' \
    'lodctr\t"Load performance monitor counters"' \
    'msiexec\t"Open an installer for .msi files"' \
    'net\t"Manage services"' \
    'notepad\t"Notepad, a simple text editor"' \
    'oleview\t"Browse and explore COM objects as well as configure DCOM"' \
    'progman\t"Open Program manager"' \
    'reg\t"Edit registry though command line"' \
    'regedit\t"Edit registry"' \
    'regsvr32\t"Register OLE components in the registry"' \
    'rpcss\t"Open rpcss.exe"' \
    'rundll32\t"Load a DLL and run an entry point"' \
    'secedit\t"Edit security configuration"' \
    'services\t"Manages services"' \
    'spoolsv\t"Print documents"' \
    'start\t"Start a program or open a file in the program"' \
    'svchost\t"Host process for services"' \
    'taskmgr\t"Open Task Manager"' \
    'uninstaller\t"Uninstall a program"' \
    'unlodctr\t"Unload performance monitor counters"' \
    'view\t"View metafiles"' \
    'wineboot\t"Reboot Wine"' \
    'winebrowser\t"Launch native OS browser or mail client"' \
    'winecfg\t"Configure wine through a GUI"' \
    'wineconsole\t"Open Windows console"' \
    'winedbg\t"Open debugger core"' \
    'winedevice\t"Manages devices"' \
    'winefile\t"Open file explorer"' \
    'winemenubuilder\t"Build Unix menu entries"' \
    'winemine\t"classic minesweeper game"' \
    'winepath\t"Translate between Windows and Unix paths formats"' \
    'winetest\t"Run DLL conformance test programs"' \
    'winevdm\t"Open DOS"' \
    'winhelp\t"Open help"' \
    'winhlp32\t"HOpen help"' \
    'winver\t"Show about information"' \
    'wordpad\t"Open WordPad"' \
    'write\t"Open WordPad"' \
    'xcopy\t"Run xcopy"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)
set -l root_condition "not __fish_seen_subcommand_from $subcommands"
complete -c $command -a "$subcommands_with_descriptions" -n $root_condition

__fish_wine__complete_cmd_subcommand $command
__fish_wine__complete_control_subcommand $command
__fish_wine__complete_eject_subcommand $command
__fish_wine__complete_explorer_subcommand $command
__fish_wine__complete_msiexec_subcommand $command
__fish_wine__complete_regedit_subcommand $command
__fish_wine__complete_start_subcommand $command
__fish_wine__complete_winemenubuilder_subcommand $command
__fish_wine__complete_winepath_subcommand $command
