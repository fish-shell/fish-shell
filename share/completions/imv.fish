complete -c imv -s h --description 'Show help message and quit'
complete -c imv -s v --description 'Show version and quit'
complete -c imv -s b --description 'Background colour (6-digit hex)' -x
complete -c imv -s c --description 'Command to run on launch' -x
complete -c imv -s d --description 'Visible overlay'
complete -c imv -s f --description 'Start fullscreen'
complete -c imv -s l --description 'List open files to stdout at exit'
complete -c imv -s n --description 'Start with the given path/index selected'
complete -c imv -s r --description 'Load directories recursively'
complete -c imv -s s --description 'Set scaling mode to use' -xa "none shrink full"
complete -c imv -s t --description 'Slideshow mode with X seconds per photo' -x
complete -c imv -s u --description 'Upscaling method' -xa "linear nearest_neighbour"
complete -c imv -s x --description 'Disable looping of input paths'
