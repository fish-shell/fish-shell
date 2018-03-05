#apt-cdrom

# magic completion safety check (do not remove this comment)
if not type -q apt-cdrom
    exit
end
complete -c apt-cdrom -s h -l help -d "Display help and exit"
complete -r -c apt-cdrom -a add -d "Add new disc to source list"
complete -x -c apt-cdrom -a ident -d "Report identity of disc"
complete -r -c apt-cdrom -s d -l cdrom -d "Mount point"
complete -f -c apt-cdrom -s r -l rename -d "Rename a disc"
complete -f -c apt-cdrom -s m -l no-mount -d "No mounting"
complete -f -c apt-cdrom -s f -l fast -d "Fast copy"
complete -f -c apt-cdrom -s a -l thorough -d "Thorough package scan"
complete -f -c apt-cdrom -s n -l no-act -d "No changes"
complete -f -c apt-cdrom -s v -l version -d "Display version and exit"
complete -r -c apt-cdrom -s c -l config-file -d "Specify config file"
complete -x -c apt-cdrom -s o -l option -d "Specify options"
