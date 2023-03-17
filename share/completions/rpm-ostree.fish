# Define subcommands for rpm-ostree
set -l subcommands apply-live compose cancel cleanup db deploy initramfs initramfs-etc install kargs override refresh-md reload rebase reset rollback status uninstall upgrade usroverlay

# File completions also need to be disabled
complete -c rpm-ostree -f

# Define auto-completion options for rpm-ostree
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a "$subcommands"

# deploy
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l unchanged-exit-77 -d 'Exit status 77 to indicate that the system is already on the specified commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -s r -l reboot -d 'Initiate a reboot after the upgrade is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l preview -d 'Download enough metadata to inspect the RPM diff, but do not actually create a new deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -s C -l cache-only -d 'Perform the operation without trying to download the target tree from the remote nor the latest packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l download-only -d 'Only download the target ostree and layered RPMs without actually performing the deployment. This can be used with a subsequent --cache-only invocation to perform the operation completely offline.'

# install
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l idempotent -d 'Do nothing if a package request is already present instead of erroring out'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s r -l reboot -d 'Initiate a reboot after the deployment is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s n -l dry-run -d 'Exit after printing the transaction rather than downloading the packages and creating a new deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l allow-inactive -d 'Allow requests for packages that are already in the base layer'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s C -l cache-only -d 'Perform the operation without trying to download the latest packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l download-only -d 'Only download the target layered RPMs without actually performing the deployment. This can be used with a subsequent --cache-only invocation to perform the operation completely offline.'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l apply-live -d 'Perform a subsequent apply-live operation to apply changes to the booted deployment'

# uninstall
complete -c rpm-ostree -n '__fish_seen_subcommand_from uninstall' -s r -l reboot -d 'Initiate a reboot after the deployment is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from uninstall' -s n -l dry-run -d 'Exit after printing the transaction rather than downloading the packages and creating a new deployment'

# rebase
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l branch -d 'Pick a branch name'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l remote -d 'Pick a remote name'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -s C -l cache-only -d 'Perform the rebase without trying to download the target tree from the remote nor the latest packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l download-only -d 'Only download the target ostree and layered RPMs without actually performing the deployment. This can be used with a subsequent --cache-only invocation to perform the operation completely offline'

# rollback
complete -c rpm-ostree -n '__fish_seen_subcommand_from rollback' -s r -l reboot -d 'Initiate a reboot after rollback is prepared'

# upgrade
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l allow-downgrade -d 'Permit deployment of chronologically older trees'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l preview -d 'Download only /usr/share/rpm in order to do a package-level diff between the two versions'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l check -d 'Just check if an upgrade is available, without downloading it or performing a package-level diff. Using this flag will force an update of the RPM metadata from the enabled repos in /etc/yum.repos.d/, if there are any layered packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -s C -l cache-only -d 'Perform the upgrade without trying to download the latest tree from the remote nor the latest packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l download-only -d 'Only download the target ostree and layered RPMs without actually performing the deployment. This can be used with a subsequent --cache-only invocation to perform the operation completely offline'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -s r -l reboot -d 'Initiate a reboot after the upgrade is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l unchanged-exit-77 -d 'Exit status 77 to indicate that the system is already up to date. This tristate return model is intended to support idempotency-oriented systems automation tools like Ansible.'

# kargs
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l editor -d 'Use an editor to modify the kernel arguments'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l append -d 'Append a kernel argument'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l append-if-missing -d 'Append a kernel argument if it is not present'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l delete -d 'Delete a kernel argument'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l delete-if-present -d 'Delete a kernel argument if it is already present'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l replace -d 'Replace an existing kernel argument'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l unchanged-exit-77 -d 'Exit status 77 to indicate that the kernel arguments have not changed'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l deploy-index -d 'Use the specified deployment index to modify kernel arguments'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l import-proc-cmdline -d 'Use the current boot kernel arguments to modify the kernel arguments'

# cleanup
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s p -l pending -d 'Remove the pending deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s r -l rollback -d 'Remove the default rollback deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s b -l base -d 'Remove any transient allocated space from interrupted operations'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s m -l repomd -d 'Clean up cached RPM repodata and partially downloaded packages'

# initramfs
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l enable -d 'Turn on client side initramfs regeneration'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l arg -d 'Append additional custom arguments to the initramfs program'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l disable -d 'Disable initramfs regeneration'

# initramfs-etc
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l track -d 'Start tracking a specific file. Can be specified multiple times.'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l untrack -d 'Stop tracking files.'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l untrack-all -d 'Stop tracking all files.'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l force-sync -d 'Generate a new deployment with the latest versions of tracked files without upgrading.'

# apply-live
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l reset -d 'Reset the filesystem tree to the booted commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l target -d 'Target an arbitrary OSTree commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l allow-replacement -d 'Enable live updates and removals for existing packages'