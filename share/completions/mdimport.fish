# completion for mdimport (macOS)

complete -c mdimport -s g -r -d 'Import files using the listed plugin'
complete -c mdimport -s V -f -d 'Print timing information for this run'
complete -c mdimport -s A -f -d 'Print out the list of all of the attributes and exit'
complete -c mdimport -s X -f -d 'Print out the schema file and exit'
complete -c mdimport -s r -f -d 'Ask the server to reimport files for UTIs claimed by the listed plugin'
complete -c mdimport -s p -f -d 'Print out performance information gathered during the run'
complete -c mdimport -s L -f -d 'Print the list of installed importers and exit'
complete -c mdimport -s d -x -a '1 2 3 4' -d 'Print debugging information'
complete -c mdimport -s n -f -d 'Dont send the imported attributes to the data store'
complete -c mdimport -s w -x -d 'Wait for the specified interval between scanning files'
complete -c mdimport -s o -r -d 'Write the imported attributes to a file'
