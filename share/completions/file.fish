#completion for file

complete -c file -s b -l brief -d 'Do not prepend filenames to output lines'
complete -c file -s c -l checking-printout -d 'Print the parsed form of the magic file'
complete -c file -s C -l compile -d 'Write an output file containing a pre-parsed version of file'
complete -c file -s h -l no-dereference -d 'Do not follow symlinks'

if test (uname) = Darwin
    complete -c file -s i -d 'Do not classify regular file contents'
    complete -c file -s I -l mime -d 'Output mime type strings instead human readable strings'
else
    complete -c file -s i -l mime -d 'Output mime type strings instead human readable strings'
end

complete -c file -s k -l keep-going -d 'Don\'t stop at the first match'
complete -c file -s L -l dereference -d 'Follow symlinks'
complete -c file -s n -l no-buffer -d 'Flush stdout after checking each file'
complete -c file -s N -l no-pad -d 'Don\'t pad filenames so that they align in the output'
complete -c file -s p -l preserve-date -d 'Attempt to preserve the access time of files analyzed'
complete -c file -s r -l raw -d 'Don\'t translate unprintable characters to octal'
complete -c file -s s -l special-files -d 'Read block and character device files too'
complete -c file -s v -l version -d 'Print the version of the program and exit'
complete -c file -s z -l uncompress -d 'Try to look inside compressed files'
complete -c file -l help -d 'Print a help message and exit'

complete -r -c file -s f -l files-from -d 'Read  the  names of the files to be examined from a file'
complete -r -c file -s F -l separator -d 'Use other string as result field separator instead of :'
complete -r -c file -s m -l magic-file -d 'Alternate list of files containing magic numbers'
