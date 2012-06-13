
complete -c ncdu -s h -d 'Print help'
complete -c ncdu -s q -d 'Quiet mode. Refresh interval 2 seconds'
complete -c ncdu -s v -d 'Print version'
complete -c ncdu -s x -d 'Same filesystem'
complete -c ncdu -l exclude -d 'Exclude files that match pattern' -r
complete -c ncdu -s X -l exclude-from -d 'Exclude files that match any pattern in file' -r
