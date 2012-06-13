__fish_complete_lpr lpr
complete -c lpr -xa "(__fish_complete_suffix .pdf)"
complete -c lpr -xa "(__fish_complete_suffix .ps)"
complete -c lpr -s H -x -d 'Specifies an alternate server' -xa '(__fish_print_hostnames)'
complete -c lpr -s C -s J -s T -x -d 'Sets the job name'
#complete -c lpr -o '\\#' -d 'Sets the number of copies to print from 1 to 100' -xa
complete -c lpr -s h -d 'Disables banner printing'
complete -c lpr -s l -d 'Specifies that the print file is already formatted for the destination and should be sent  without filtering. This option is equivalent to "-o raw"'
complete -c lpr -s p -d 'Specifies  that  the  print file should be formatted with a shaded header with the date, time, job name, and page number. This option is equivalent to "-o  prettyprint"  and  is  only  useful  when printing text files'
complete -c lpr -s q -d 'Hold job for printing'
complete -c lpr -s r -d 'Specifies that the named print files should be deleted after printing them'
