
complete -c logkeys -s s -l start -d 'Starts the keylogging daemon'
complete -c logkeys -s k -l kill -d 'Terminates the logkeys daemon'
complete -c logkeys -s o -l output -d 'Set ouput LOGFILE'
complete -c logkeys -s m -l keymap -n '__fish_not_contain_opt -s u' -d 'Use file as input KEYMAP'
complete -c logkeys -s d -l device -d 'Use DEVICE as keyboard input'
complete -c logkeys -s u -l us-keymap -n '__fish_not_contain_opt -s m' -d 'Treat keyboard as standard US keyboard'
complete -c logkeys -l export-keymap -d 'Export dynamic KEYMAP to file'
complete -c logkeys -l no-func-keys -d 'Log only character key presses'
complete -c logkeys -l no-timestamps -d 'No timestamp to each line of log'
complete -c logkeys -l post-size -d 'On SIZE, rotate current logfile'
complete -c logkeys -l post-http -d 'POST the log file to URL'

