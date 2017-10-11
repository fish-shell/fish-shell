#completion for totem

complete -c totem -l usage -d 'Output a brief synopsis of command options then quit'
complete -c totem -s '?' -l help -d 'Output  a longer help message then quit'
complete -c totem -l version -d 'Output version information then quit'
complete -c totem -l play-pause -d 'Tell any running totem instance: Toggle between play and pause'
complete -c totem -l play -d 'Tell any running totem instance: Play'
complete -c totem -l pause -d 'Tell any running totem instance: Pause'
complete -c totem -l next -d 'Tell any running totem instance: Skip to next'
complete -c totem -l previous -d 'Tell any running totem instance: Skip to previous'
complete -c totem -l seek-fwd -d 'Tell any running totem instance: Seek forwards 15 sec'
complete -c totem -l seek-bwd -d 'Tell any running totem instance: Seek backwards 15 sec'
complete -c totem -l volume-up -d 'Tell any running totem instance: Raise volume by 8%'
complete -c totem -l volume-down -d 'Tell any running totem instance: Lower volume by 8%'
complete -c totem -l fullscreen -d 'Tell any running totem instance: Toggle fullscreen'
complete -c totem -l quit -d 'Tell any running totem instance: Quit'

complete -r -c totem -l enqueue -d 'Tell any running totem instance: Add to playlist'
complete -r -c totem -l replace -d 'Tell any running totem instance: Play from playlist'
