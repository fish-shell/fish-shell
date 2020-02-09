# Completions for flatpak, an "Application deployment framework for desktop apps"
# (http://flatpak.org)
set -l commands install update uninstall list info run override make-current enter document-{export,unexport,info,list} \
    remote-{add,modify,delete,list,ls} build build-{init,finish,export,bundle,import-bundle,sign,update-repo}

complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a install -d 'Install an application or runtime from a remote'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a update -d 'Update an installed application or runtime'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a uninstall -d 'Uninstall an installed application or runtime'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a list -d 'List installed apps and/or runtimes'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a info -d 'Show info for installed app or runtime'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a run -d 'Run an application'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a override -d 'Override permissions for an application'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a make-current -d 'Specify default version to run'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a enter -d 'Enter the namespace of a running application'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a document-export -d 'Grant an application access to a specific file'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a document-unexport -d 'Revoke access to a specific file'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a document-info -d 'Show information about a specific file'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a document-list -d 'List exported files'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a remote-add -d 'Add a new remote repository (by URL)'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a remote-modify -d 'Modify properties of a configured remote'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a remote-delete -d 'Delete a configured remote'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a remote-list -d 'List all configured remotes'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a remote-ls -d 'List contents of a configured remote'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-init -d 'Initialize a directory for building'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build -d 'Run a build command inside the build dir'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-finish -d 'Finish a build dir for export'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-export -d 'Export a build dir to a repository'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-bundle -d 'Create a bundle file from a build directory'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-import-bundle -d 'Import a bundle file'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-sign -d 'Sign an application or runtime'
complete -f -c flatpak -n "not __fish_seen_subcommand_from $commands" -a build-update-repo -d 'Update the summary file in a repository'

complete -f -c flatpak -n "__fish_seen_subcommand_from run" -a "(flatpak list --app | string match -r '\S+')"
complete -f -c flatpak -n "__fish_seen_subcommand_from info uninstall" -a "(flatpak list | string match -r '\S+')"
complete -f -c flatpak -n "__fish_seen_subcommand_from remote-ls remote-modify remote-delete" -a "(flatpak remote-list | string match -r '\S+')"

# Note that "remote-ls" opens an internet connection, so we don't want to complete "install"
# Plenty of the other stuff is too free-form to complete (e.g. remote-add).
complete -f -c flatpak -s h -l help
