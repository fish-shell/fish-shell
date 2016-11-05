# completion for nvram (macOS)

function __fish_nvram_variables
    command nvram -p
end

complete -c nvram -s x -f -d 'Use  XML  format  for  reading  and  writing variables'
complete -c nvram -s p -f -d 'Print all of the firmware variables'
complete -c nvram -s f -r -d 'Set firmware variables from a text file'
complete -c nvram -s d -x -a '(__fish_nvram_variables)' -d 'Deletes the named firmware variable'
complete -c nvram -s c -f -d 'Delete all of the firmware variable'
complete -c nvram -x -a '(__fish_nvram_variables)'
