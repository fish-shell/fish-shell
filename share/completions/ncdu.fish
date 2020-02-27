complete -c ncdu -s h -l help -d 'Print help'
complete -c ncdu -s q -d 'Quiet mode. Refresh interval 2 seconds'
complete -c ncdu -s v -s V -l version -d 'Print version'
complete -c ncdu -s x -d 'Same filesystem'
complete -c ncdu -s e -d 'Enable extended information'
complete -c ncdu -s r -d 'Read only'
complete -c ncdu -s o -d 'Export scanned directory' -r
complete -c ncdu -s f -d 'Import scanned directory' -r
complete -c ncdu -s 0 -s 1 -s 2 -d 'UI to use when scanning (0=none,2=full ncurses)'
complete -c ncdu -l si -d 'Use base 10 prefixes instead of base 2'
complete -c ncdu -l exclude -d 'Exclude files that match pattern' -x
complete -c ncdu -s X -l exclude-from -d 'Exclude files that match any pattern in file' -r
complete -c ncdu -s L -l follow-symlinks -d 'Follow symlinks (excluding dirs)'
complete -c ncdu -l exclude-caches -d 'Exclude dirs containing CACHEDIR.TAG'
complete -c ncdu -l confirm-quit -d 'Prompt before exiting ncdu'
complete -c ncdu -l color -d 'Set color scheme' -x
