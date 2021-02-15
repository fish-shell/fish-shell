complete -c bectl -e

complete -c bectl -s "h?" -x

complete -c bectl -f
complete -c bectl -a activate -d "Activate the given beName as the default boot filesystem"
complete -c bectl -a check -d "Performs a silent sanity check on the current system"
complete -c bectl -a create -d "Create a new boot environment named newBeName"
complete -c bectl -a destroy -d "Destroy the given beName boot environment or beName@snapshot snapshot" 
complete -c bectl -a export -d "Export sourceBe to stdout(4)"
complete -c bectl -a import -d "Import targetBe from stdin(4)"
complete -c bectl -a jail -d "Create a jail of the given boot environment"
complete -c bectl -a list -d "Display all boot environments"
complete -c bectl -a mount -d "Temporarily mount the boot environment"
complete -c bectl -a rename -d "Rename the given boot env" 
complete -c bectl -a unjail -d "Destroy the jail created from the given boot environment"
complete -c bectl -a ujail -w bectl -a unjail
complete -c bectl -a unmount -d "Unmount the given boot environment"

