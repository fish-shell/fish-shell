function __fish_wine_explorer__complete_desktop_arg
    set -l current_token (commandline -tc)

    switch "$current_token"
        case '/desktop=*x*'
            return
        case '/desktop=*'
            printf '%sx' "$current_token"
    end
end

set -l command wine
complete -c $command -f

complete -c $command -l help -d 'Show help'
complete -c $command -l version -d 'Show version'

set -l subcommands_with_descriptions 'cacls\t"Edit ACLs"' \
    'clock\t"Open the clock"' \
    'cmd\t"Command the command prompt"' \
    'cmdlgtst\t"commdlg.dll test jig"' \
    'control\t"Open control panel"' \
    'eject\t"Eject optical discs"' \
    'expand\t"Expand cabinet files"' \
    'explorer\t"Open Explorer"' \
    'hh\t"View HTML help"' \
    'icinfo\t"List installed video compressors"' \
    'iexplore\t"Open Internet Explorer"' \
    'lodctr\t"Load performance monitor counters"' \
    'msiexec\t"Open an installer for .msi files"' \
    'net\t"Manage services"' \
    'notepad\t"Notepad, a simple text editor"' \
    'oleview\t"Browse and explore COM objects as well as configure DCOM"' \
    'progman\t"Open Program manager"' \
    'reg\t"Edit registry"' \
    'regedit\t"Registry Editor"' \
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
    'view\t"Metafile viewer"' \
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
    'winhelp\t"Help viewer"' \
    'winhlp32\t"Help viewer"' \
    'winver\t"Show about information"' \
    'wordpad\t"Open WordPad"' \
    'write\t"Open WordPad"' \
    'xcopy\t"Run xcopy"'

set -l subcommands (string replace --regex '\\\t.+' '' -- $subcommands_with_descriptions)
set -l root_condition "not __fish_seen_subcommand_from $subcommands"
complete -c $command -a "$subcommands_with_descriptions" -n $root_condition

set -l cmd_condition '__fish_seen_subcommand_from cmd'
complete -c $command -a '(__fish_cmd__complete_args)' -n $cmd_condition

set -l c_and_k_condition 'not __fish_seen_argument -w c -w k'

complete -c $command -a /c \
    -d 'Run the command and stop' \
    -n "$cmd_condition && $c_and_k_condition"

complete -c $command -a /k \
    -d 'Run the command and continue' \
    -n "$cmd_condition && $c_and_k_condition"

complete -c $command -a /s \
    -d 'Modify the treatment of string after /c or /k' \
    -n $cmd_condition

complete -c $command -a /q -d 'Turn echo off' -n $cmd_condition

complete -c $command -a /d \
    -d 'Disable execution of AutoRun commands' \
    -n $cmd_condition

set -l a_and_u_condition 'not __fish_seen_argument -w a -w u'

complete -c $command -a /a \
    -d 'Format internal command output as ANSI' \
    -n "$cmd_condition && $a_and_u_condition"

complete -c $command -a /u \
    -d 'Format internal command output as Unicode' \
    -n "$cmd_condition && $a_and_u_condition"

complete -c $command -a /t \
    -d 'Set the background and foreground color' \
    -n $cmd_condition

complete -c $command -a /e -d 'Manage command extensions' -n $cmd_condition

complete -c $command -a /f \
    -d 'Manage file and directory name completion' \
    -n $cmd_condition

complete -c $command -a /v \
    -d 'Manage delayed environment variable expansion' \
    -n $cmd_condition

complete -c $command -a '/?' -d 'Show help' -n $cmd_condition

set -l control_condition '__fish_seen_subcommand_from control'

complete -c $command \
    -a 'COLOR DATE/TIME DESKTOP INTERNATIONAL KEYBOARD MOUSE PORTS PRINTERS' \
    -n $control_condition

set -l eject_condition '__fish_seen_subcommand_from eject'
complete -c $command -s h -d 'Show help' -n $eject_condition
complete -c $command -s a -d 'Eject all the CD drives' -n $eject_condition
complete -c $command -s u -d 'Unmount the CD drives' -n $eject_condition

set -l explorer_condition '__fish_seen_subcommand_from explorer'

complete -c $command -a '(__fish_wine_explorer__complete_desktop_arg)' \
    -n $explorer_condition

complete -c $command -a /n -d 'Use the single pain view' -n $explorer_condition
complete -c $command -a /e, -d 'Use the default view' -n $explorer_condition

complete -c $command -a /root, -d 'Specify the root level of a view' \
    -n $explorer_condition

complete -c $command -a /select, -d 'Specify the selection in a view' \
    -n $explorer_condition

complete -c $command -a /desktop= -d 'Specify the desktop name' \
    -n $explorer_condition

# put msiexec completion here

set -l regedit_condition '__fish_seen_subcommand_from regedit'
complete -c $command -a '/?' -d 'Show help' -n $regedit_condition

complete -c $command -a /L \
    -d 'Specify the location of system.dat file' \
    -n $regedit_condition

complete -c $command -a /R \
    -d 'Specify the location of user.dat file' \
    -n $regedit_condition

complete -c $command -a /C \
    -d 'Import contents of a registry file' \
    -n $regedit_condition

complete -c $command -a /D \
    -d 'Delete a registry key' \
    -n $regedit_condition

complete -c $command -a /E \
    -d 'Export contents to a registry file' \
    -n $regedit_condition

complete -c $command -a /S \
    -d 'Do not display messages' \
    -n $regedit_condition

complete -c $command -a /V \
    -d 'Launch the GUI in an advanced mode' \
    -n $regedit_condition

complete -c $command -a '(__fish_reg__complete_keys)' \
    -n $regedit_condition

set -l start_condition '__fish_seen_subcommand_from start'
complete -c $command -a '/?' -d 'Show help' -n $start_condition

complete -c $command -a /d \
    -d 'Specify the directory for a program' \
    -n $start_condition

complete -c $command -a /b \
    -d "Don't create the new console for a progra" \
    -n $start_condition

complete -c $command -a /i \
    -d 'Clear the environment for a program' \
    -n $start_condition

complete -c $command -a /min \
    -d 'Start a program in the minimized window' \
    -n $start_condition

complete -c $command -a /max \
    -d 'Start a program in the minimized window' \
    -n $start_condition

complete -c $command -a /low \
    -d 'Start a program in the idle priority class' \
    -n $start_condition

complete -c $command -a /normal \
    -d 'Start a program in the normal priority class' \
    -n $start_condition

complete -c $command -a /high \
    -d 'Start a program in the high priority class' \
    -n $start_condition

complete -c $command -a /realtime \
    -d 'Start a program in the realtime priority class' \
    -n $start_condition

complete -c $command -a /abovenormal \
    -d 'Start a program in the abovenormal priority class' \
    -n $start_condition

complete -c $command -a /belownormal \
    -d 'Start a program in the belownormal priority class' \
    -n $start_condition

complete -c $command -a /node \
    -d 'Specify the NUMA node for a program' \
    -n $start_condition

complete -c $command -a /affinity \
    -d 'Specify the affinity mask for a program' \
    -n $start_condition

complete -c $command -a /wait \
    -d 'Wait for a program to exit' \
    -n $start_condition

complete -c $command -a /unix \
    -d 'Use the Unix filename for a program' \
    -n $start_condition

complete -c $command -a /w \
    -d 'Wait till the shortcut can be created' \
    -n '__fish_seen_subcommand_from winemenubuilder'

set -l winepath_condition '__fish_seen_subcommand_from winepath'
complete -c $command -s h -d 'Show help' -n $winepath_condition
complete -c $command -s v -d 'Show version' -n $winepath_condition

complete -c $command -s u -l unix \
    -d 'Convert a Windows path to the Unix one' \
    -n $winepath_condition

complete -c $command -s w -l windows \
    -d 'Convert a Unix path to the Windows one' \
    -n $winepath_condition

complete -c $command -s l -l long \
    -d 'Convert a Windows path to the long format' \
    -n $winepath_condition

complete -c $command -s s -l short \
    -d 'Convert a Windows path to the short format' \
    -n $winepath_condition
