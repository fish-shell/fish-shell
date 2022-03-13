if chown --version &>/dev/null # gee, and you is not eunichs
    complete chown -s c -l changes -d "Output diagnostic for changed files"
    complete chown -l dereference -d "Dereference symbolic links"
    complete chown -s h -l no-dereference -d "Do not dereference symbolic links"
    complete chown -l from -d "Change from owner/group"
    complete chown -s f -l silent -d "Suppress errors"
    complete chown -l reference -d "Use same owner/group as file" -r
    complete chown -s R -l recursive -d "Operate recursively"
    complete chown -s v -l verbose -d "Output diagnostic for every file"
    complete chown -l help -d "Display help and exit"
    complete chown -l version -d "Display version and exit"
else
    complete chown -s R -d 'Operate recursively'
    complete chown -s f -d "Suppress errors"
    complete chown -s h -d "Do not dereference symbolic links"
    complete chown -s n -d "uid/gid is numeric; avoid lookup"
    complete chown -s v -d "Output filenames as modified"
    complete chown -s x -d "Don't traverse fs mount points"
end

complete chown -s H -d "Follow given symlinks with -R"
complete chown -s L -d "Follow all symlinks with -R"
complete chown -s P -d "Follow no symlinks with -R"
complete chown -d User -a "(__fish_print_users):"
complete chown -d User -a "(string match -r -- '.*:' (commandline -ct))(__fish_complete_groups)"
