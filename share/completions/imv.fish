complete -c imv -s h -d 'Show help message and quit'
complete -c imv -s v -d 'Show version and quit'
complete -c imv -s b -d 'Background colour (6-digit hex)' -x
complete -c imv -s c -d 'Command to run on launch' -x
complete -c imv -s d -d 'Visible overlay'
complete -c imv -s f -d 'Start fullscreen'
complete -c imv -s l -d 'List open files to stdout at exit'
complete -c imv -s n -d 'Start with the given path/index selected'
complete -c imv -s r -d 'Load directories recursively'
complete -c imv -s s -d 'Set scaling mode to use' -xa "none shrink full"
complete -c imv -s t -d 'Slideshow mode with X seconds per photo' -x
complete -c imv -s u -d 'Upscaling method' -xa "linear nearest_neighbour"
complete -c imv -s x -d 'Disable looping of input paths'
