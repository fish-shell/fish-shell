#completion for totem

complete -c totem -l usage --description 'Output a brief synopsis of command options then quit'
complete -c totem -s '?' -l help --description 'Output  a longer help message then quit'
complete -c totem -l version --description 'Output version information then quit'
complete -c totem -l play-pause --description 'Tell any running totem instance: Toggle between play and pause'
complete -c totem -l play --description 'Tell any running totem instance: Play'
complete -c totem -l pause --description 'Tell any running totem instance: Pause'
complete -c totem -l next --description 'Tell any running totem instance: Skip to next'
complete -c totem -l previous --description 'Tell any running totem instance: Skip to previous'
complete -c totem -l seek-fwd --description 'Tell any running totem instance: Seek forwards 15 sec'
complete -c totem -l seek-bwd --description 'Tell any running totem instance: Seek backwards 15 sec'
complete -c totem -l volume-up --description 'Tell any running totem instance: Raise volume by 8%'
complete -c totem -l volume-down --description 'Tell any running totem instance: Lower volume by 8%'
complete -c totem -l fullscreen --description 'Tell any running totem instance: Toggle fullscreen'
complete -c totem -l quit --description 'Tell any running totem instance: Quit'

complete -r -c totem -l enqueue --description 'Tell any running totem instance: Add to playlist'
complete -r -c totem -l replace --description 'Tell any running totem instance: Play from playlist'
