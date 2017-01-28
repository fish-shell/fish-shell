# htop is an interactive process viewer.
# See: http://hisham.hm/htop

complete -c htop -l delay -s d -d 'Update interval' -x
complete -c htop -l no-color -s C -d 'Start htop in monochrome mode'
complete -c htop -l no-colour -d 'Start htop in monochrome mode'
complete -c htop -l help -s h -d 'Show help and exit'
complete -c htop -l pid -s p -d 'Show only given PIDs' -x -a '(__fish_append , (__fish_complete_pids))'
complete -c htop -l user -s u -d 'Monitor given user' -x -a '(__fish_complete_users)'
complete -c htop -l sort-key -d 'Sort column' -xa '(htop --sort-key help)'
complete -c htop -l version -s v -d 'Show version and exit'
