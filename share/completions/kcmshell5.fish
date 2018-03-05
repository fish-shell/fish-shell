
# magic completion safety check (do not remove this comment)
if not type -q kcmshell5
    exit
end
complete -c kcmshell5 -f -l list -d "List available modules"
complete -c kcmshell5 -f -l author -d "Show author information"
complete -c kcmshell5 -f -l license -d "Show license information"
complete -c kcmshell5 -f -l lang -d "Set a language"
complete -c kcmshell5 -f -l silent -d "Don't show main window"
complete -c kcmshell5 -f -l args -d "Arguments to the module"
complete -c kcmshell5 -f -l icon -d "Icon for the module"
complete -c kcmshell5 -f -l caption -d "Title for the window"
complete -c kcmshell5 -f -l version -s v -d "Show version information and exit"
complete -c kcmshell5 -f -l help -s h -d "Show help information and exit"
complete -c kcmshell5 -f -a "(kcmshell5 --list | tail -n +2 | string replace -r -- ' +-' \t)"
