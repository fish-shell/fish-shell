## Listing options
complete -c tree -s a -d 'All files are listed'
complete -c tree -s d -d 'List directories only'
complete -c tree -s l -d 'Follow symbolic links like directories'
complete -c tree -s f -d 'Print the full path prefix for each file'
complete -c tree -s x -d 'Stay on current filesystem only'
complete -c tree -s L -x -d 'Descend only level directories deep'
complete -c tree -s R -d 'Rerun tree when max dir level reached'
complete -c tree -s P -r -d 'List only those files that match the pattern given'
complete -c tree -s I -r -d 'Do not list files that match the given pattern'
complete -c tree -l gitignore -d 'Filter by using .gitignore files'
complete -c tree -l ignore-case -d 'Ignore case when pattern matching'
complete -c tree -l matchdirs -d 'Include directory names in -P pattern matching'
complete -c tree -l metafirst -d 'Print meta-data at the beginning of each line'
complete -c tree -l prune -d 'Prune empty directories from the output'
complete -c tree -l info -d 'Print information about files found in .info files'
complete -c tree -l noreport -d 'Turn off file/directory count at end of tree listing'
complete -c tree -l charset -x -d 'Use charset X for terminal/HTML and indentation line output'
complete -c tree -l filelimit -r -d 'Do not descend dirs with more than # files in them'
complete -c tree -s o -r -d 'Output to file instead of stdout'

## File options
complete -c tree -s q -d 'Print non-printable characters as \'?\''
complete -c tree -s N -d 'Print non-printable characters as is'
complete -c tree -s Q -d 'Quote filenames with double quotes'
complete -c tree -s p -d 'Print the protections for each file'
complete -c tree -s u -d 'Displays file owner or UID number'
complete -c tree -s g -d 'Displays file group owner or GID number'
complete -c tree -s s -d 'Print the size in bytes of each file'
complete -c tree -s h -d 'Print the size in a more human readable way'
complete -c tree -l si -d 'Like -h, but use in SI units (powers of 1000)'
complete -c tree -l du -d 'Compute size of directories by their contents'
complete -c tree -s D -d 'Print the date of last modification or (-c) status change'
complete -c tree -l timefmt -x -d 'Print and format time according to the format <f>'
complete -c tree -s F -d 'Appends \'/\', \'=\', \'*\', \'@\', \'|\' or \'>\' as per ls -F'
complete -c tree -l inodes -d 'Print inode number of each file'
complete -c tree -l device -d 'Print device ID number to which each file belongs'

## Sorting options
complete -c tree -s v -d 'Sort files alphanumerically by version'
complete -c tree -s t -d 'Sort files by last modification time'
complete -c tree -s c -d 'Sort files by last status change time'
complete -c tree -s U -d 'Leave files unsorted'
complete -c tree -s r -d 'Reverse the order of the sort'
complete -c tree -l dirsfirst -d 'List directories before files (-U disables)'
complete -c tree -l filesfirst -d 'List files before directories (-U disables)'
complete -c tree -l sort -d 'Select sort' -xa 'name version size mtime ctime'

## Graphics options
complete -c tree -s i -d 'Don\'t print indentation lines'
complete -c tree -s A -d 'Print ANSI lines graphic indentation lines'
complete -c tree -s S -d 'Print with CP437 (console) graphics indentation lines'
complete -c tree -s n -d 'Turn colorization off always (-C overrides)'
complete -c tree -s C -d 'Turn colorization on always'

## XML/HTML/JSON options
complete -c tree -s X -d 'Prints out an XML representation of the tree'
complete -c tree -s J -d 'Prints out an JSON representation of the tree'
complete -c tree -s H -r -d 'Prints out HTML format with baseHREF as top directory'
complete -c tree -s T -r -d 'Replace the default HTML title and H1 header with string'
complete -c tree -l nolinks -d 'Turn off hyperlinks in HTML output'

## Input options
complete -c tree -l fromfile -r -d 'Reads paths from files (.=stdin)'

## Miscellaneous options
complete -c tree -l version -d 'Print version and exit'
complete -c tree -l help -d 'Print usage and this help message and exit'
