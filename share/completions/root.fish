complete -c root -s b -d 'Run in batch mode'
complete -c root -s n -d 'Do not execute logon and logoff macros'
complete -c root -s q -d 'Exit after processing commandline macro files'
complete -c root -s l -d 'Do not show splashscreen'
complete -c root -s x -d 'Exit on exception'
complete -c root -s h -s \? -l help -d 'Print help'
complete -c root -o config -d 'Print ./configure options'
complete -c root -o memstat -d 'Run with memory usage monitoring'
complete -c root -k -a "(__fish_complete_suffix .root)"
