# Completions for flatpak, an "Application deployment framework for desktop apps"

function __fish_flatpak_subcommands
    printf "%s\n" \
        build-bundle\t"Create a bundle file from a build directory" \
        build-commit-from\t"Create new commit based on existing ref" \
        build-export\t"Export a build dir to a repository" \
        build-finish\t"Finish a build dir for export" \
        build-import-bundle\t"Import a bundle file" \
        build-init\t"Initialize a directory for building" \
        build-sign\t"Sign an application or runtime" \
        build-update-repo\t"Update the summary file in a repository" \
        build\t"Run a build command inside the build dir" \
        config\t"Configure flatpak" \
        create-usb\t"Put applications or runtimes onto removable media" \
        document-export\t"Grant an application access to a specific file" \
        document-info\t"Show information about a specific file" \
        document-unexport\t"Revoke access to a specific file" \
        documents\t"List exported files" \
        enter\t"Enter the namespace of a running application" \
        history\t"Show history" \
        info\t"Show info for installed app or runtime" \
        install\t"Install an application or runtime from a remote" \
        kill\t"Stop a running application" \
        list\t"List installed apps and/or runtimes" \
        make-current\t"Specify default version to run" \
        mask\t"Mask out updates and automatic installation" \
        override\t"Override permissions for an application" \
        permission-remove\t"Remove item from permission store" \
        permission-reset\t"Reset app permissions" \
        permission-set\t"Set permissions" \
        permission-show\t"Show app permissions" \
        permissions\t"List permissions" \
        ps\t"Enumerate running applications" \
        remote-add\t"Add a new remote repository (by URL)" \
        remote-delete\t"Delete a configured remote" \
        remote-info\t"Show information about a remote app or runtime" \
        remote-ls\t"List contents of a configured remote" \
        remote-modify\t"Modify properties of a configured remote" \
        remotes\t"List all configured remotes" \
        repair\t"Repair flatpak installation" \
        repo\t"Show information about a repo" \
        run\t"Run an application" \
        search\t"Search for remote apps/runtimes" \
        uninstall\t"Uninstall an installed application or runtime" \
        update\t"Update an installed application or runtime"
end
complete -c flatpak -n __fish_is_first_token -xa "(__fish_flatpak_subcommands)"

# A flatpak bug on some systems seems to cause it to assume output is a tty and always emit pretty-formatted output, with
# a variable number of spaces for alignment, trailing CR, and ANSI formatting sequences (see #10514).
#
# This wrapper function is akin to `flatpak $argv[1] $argv[2..]` but sanitizes the output to make it completion-friendly.
function __fish_flatpak
    # We can't rely on the first line being the headers because modern flatpak will omit it when running in a "subshell".
    # Based off our invocations, we expect the first column of actual output to never contain a capitalized letter
    # (unlike the header row).
    flatpak $argv | string replace -rf '^([^A-Z]+)(?: +|\t)(.*?)\s*$' '$1\t$2'
end

complete -f -c flatpak -n "__fish_seen_subcommand_from run" -a "(__fish_flatpak list --app --columns=application,name)"
complete -f -c flatpak -n "__fish_seen_subcommand_from info uninstall" -a "(__fish_flatpak list --columns=application,name)"
complete -f -c flatpak -n "__fish_seen_subcommand_from enter kill" -a "(__fish_flatpak ps --columns=instance,application)"
complete -f -c flatpak -n "__fish_seen_subcommand_from remote-info remote-ls remote-modify remote-delete" -a "(__fish_flatpak remotes --columns=name,title)"

complete -c flatpak -n '__fish_seen_subcommand_from install' -xa "(__fish_flatpak remote-ls --columns=application,name)"

# Plenty of the other stuff is too free-form to complete (e.g. remote-add).
complete -f -c flatpak -s h -l help
