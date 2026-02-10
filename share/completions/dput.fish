complete -c dput -s c -l config -d "Configuration file" -r
complete -c dput -s d -l debug -d "Display debugging messages"
complete -c dput -s D -l dinstall -d "Request dry-run of dinstall after upload"
complete -c dput -s f -l force -d "Disable check for unique upload"
complete -c dput -s h -l help -d "Display help"
complete -c dput -s H -l host-list -d "Display hosts listed in configuration"
complete -c dput -s l -l lintian -d "Enable the Lintian verification check"
complete -c dput -s U -l no-upload-log -d "Do not write a log file for uploads"
complete -c dput -s o -l check-only -d "Don't check the signatures or upload the files"
complete -c dput -s p -l print -d "Print the dput configuration"
complete -c dput -s P -l passive -d "Use passive mode for FTP"
complete -c dput -s s -l simulate -d "Do not upload the files"
complete -c dput -s e -l delayed -d "Upload to a delayed queue" -r
complete -c dput -s v -l version -d "Print the version"
complete -c dput -s V -l check-version -d "Enable the local install check"

complete -c dput -x -a '(dput --host-list | string match --regex --groups-only "(.+) => .*")' -d "Upload host" -n __fish_is_first_arg
# dput validates that filenames end in .changes
complete -c dput -x -a '(complete -C"\'\' $(commandline -ct)" | string match -re "(?:/|.changes)\$")'
