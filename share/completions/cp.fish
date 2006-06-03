complete -c cp -s a -l archive -d (N_ "Same as -dpR")
complete -c cp -s b -l backup -d (N_ "Make backup of each existing destination file") -a "none off numbered t existing nil simple never"
complete -c cp -l copy-contents -d (N_ "Copy contents of special files when recursive")
complete -c cp -s d -d (N_ "Same as --no-dereference --preserve=link")
complete -c cp -s f -l force -d (N_ "Do not prompt before overwriting")
complete -c cp -s i -l interactive -d (N_ "Prompt before overwrite")
complete -c cp -s H -d (N_ "Follow command-line symbolic links")
complete -c cp -s l -l link -d (N_ "Link files instead of copying")
complete -c cp -l strip-trailing-slashes -d (N_ "Remove trailing slashes from source")
complete -c cp -s S -l suffix -r -d (N_ "Backup suffix")
complete -c cp -s t -l target-directory -d (N_ "Target directory") -x -a "(__fish_complete_directory (commandline -ct) 'Target directory')"
complete -c cp -s u -l update -d (N_ "Do not overwrite newer files")
complete -c cp -s v -l verbose -d (N_ "Verbose mode")
complete -c cp -l help -d (N_ "Display help and exit")
complete -c cp -l version -d (N_ "Display version and exit")
complete -c cp -s L -l dereference -d (N_ "Always follow symbolic links")
complete -c cp -s P -l no-dereference -d (N_ "Never follow symbolic links")
complete -c cp -s p  -d (N_ "Same as --preserve=mode,ownership,timestamps")
complete -c cp -l preserve -d (N_ "Preserve the specified attributes and security contexts, if possible") -a "mode ownership timestamps links all"
complete -c cp -l no-preserve -r -d (N_ "Don't preserve the specified attributes") -a "mode ownership timestamps links all"
complete -c cp -l parents -d (N_ "Use full source file name under DIRECTORY")
complete -c cp -s r -s R -l recursive -d (N_ "Copy directories recursively")
complete -c cp -l remove-destination -d (N_ "Remove each existing destination file before attempting to open it (contrast with --force)")
complete -c cp -l sparse -r -d (N_ "Control creation of sparse files") -a "always auto never"
complete -c cp -s s -l symbolic-link -d (N_ "Make symbolic links instead of copying")
complete -c cp -s T -l no-target-directory -d (N_ "Treat DEST as a normal file")
complete -c cp -s x -l one-file-system -d (N_ "Stay on this file system")
complete -c cp -s X -l context -r -d (N_ "Set security context of copy to CONTEXT")
