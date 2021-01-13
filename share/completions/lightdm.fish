# Completions for the lightdm command

complete -c lightdm -f
complete -c lightdm -s h -l help -x -d "Show help options"
complete -c lightdm -s c -l config -r -F -d "Use configuration file"
complete -c lightdm -s d -l debug -d "Print debugging messages"
complete -c lightdm -l test-mode -d "Skip things requiring root access"
complete -c lightdm -l pid-file -r -F -d "File to write PID into"
complete -c lightdm -l xsessions-dir -r -F -d "Directory to load X sessions from"
complete -c lightdm -l xgreeters-dir -r -F -d "Directory to load X greeters from"
complete -c lightdm -l log-dir -r -F -d "Directory to write logs to"
complete -c lightdm -l run-dir -r -F -d "Directory to store running state"
complete -c lightdm -l cache-dir -r -F -d "Directory to cache information"
complete -c lightdm -s v -l version -x -d "Show release version"
