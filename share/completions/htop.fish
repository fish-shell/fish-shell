# htop is an interactive process viewer.
# See: https://htop.dev

complete -c htop -l delay -s d -d 'Update interval' -x
complete -c htop -l no-color -s C -d 'Start htop in monochrome mode'
complete -c htop -l no-colour -d 'Start htop in monochrome mode'
complete -c htop -l filter -s F -d 'Filter processes by terms matching the commands' -x
complete -c htop -l help -s h -d 'Show help and exit'
complete -c htop -l pid -s p -d 'Show only given PIDs' -xa '(__fish_append , (__fish_complete_pids))'
complete -c htop -l sort-key -s s -d 'Sort column' -xa '(htop --sort-key help | sed -E "s/^\s*([^[:space:]]*)\s*(.*)/\1\t\2/")'
complete -c htop -l user -s u -d 'Monitor given user' -xa '(__fish_complete_users)'
complete -c htop -l no-unicode -s U -d 'Do not use unicode but ASCII characters for graph meters'
complete -c htop -l no-mouse -s M -d 'Disable support of mouse control'
complete -c htop -l readonly -d 'Disable all system and process changing features'
complete -c htop -l version -s V -d 'Show version and exit'
complete -c htop -l tree -s t -d 'Show processes in tree view'
complete -c htop -l highlight-changes -s H -d 'Highlight new and old processes' -x
complete -c htop -l drop-capabilities -d 'Drop unneeded Linux capabilities (Requires libpcap support)' -xka "
		off
		basic
		strict
	"
