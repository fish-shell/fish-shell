# Define subcommands for rpm-ostree
set -l subcommands apply-live compose cancel cleanup db deploy initramfs initramfs-etc install kargs override refresh-md reload rebase reset rollback status uninstall upgrade usroverlay

# File completions also need to be disabled
complete -c rpm-ostree -f

# Define auto-completion options for rpm-ostree
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a "$subcommands"

# deploy
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l unchanged-exit-77 -d 'Exit w/ code 77 if system already on specified commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -s r -l reboot -d 'Reboot after upgrade is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l preview -d 'Download enough metadata to diff RPM w/out deploying'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -s C -l cache-only -d 'Perform operation w/out updating from remotes'
complete -c rpm-ostree -n '__fish_seen_subcommand_from deploy' -l download-only -d 'Download targeted ostree/layered RPMs w/out deploying'

# install
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l idempotent -d 'Don\'t error if package is already present'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s r -l reboot -d 'Reboot after deployment is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s n -l dry-run -d 'Exit after printing transaction (don\'t download and deploy)'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l allow-inactive -d 'Allow packages already in base layer'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -s C -l cache-only -d 'Don\'t download latest packages'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l download-only -d 'Download targeted layered RPMs w/out deploying'
complete -c rpm-ostree -n '__fish_seen_subcommand_from install' -l apply-live -d 'Apply changes to booted deployment'

# uninstall
complete -c rpm-ostree -n '__fish_seen_subcommand_from uninstall' -s r -l reboot -d 'Reboot after deployment is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from uninstall' -s n -l dry-run -d 'Exit after printing transaction (don\'t download and deploy)'

# rebase
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l branch -d 'Pick a branch name'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l remote -d 'Pick a remote name'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -s C -l cache-only -d 'Perform rebase w/out downloading latest'
complete -c rpm-ostree -n '__fish_seen_subcommand_from rebase' -l download-only -d 'Download targeted ostree/layered RPMs w/out deploying'

# rollback
complete -c rpm-ostree -n '__fish_seen_subcommand_from rollback' -s r -l reboot -d 'Reboot after rollback is prepared'

# upgrade
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l allow-downgrade -d 'Permit deployment of older trees'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l preview -d 'Minimal download in order to do a package-level version diff'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l check -d 'Check if upgrade is available w/out downloading it'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -s C -l cache-only -d 'Upgrade w/out updating to latest'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l download-only -d 'Download targeted ostree/layered RPMs w/out deploying'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -s r -l reboot -d 'Reboot after upgrade is prepared'
complete -c rpm-ostree -n '__fish_seen_subcommand_from upgrade' -l unchanged-exit-77 -d 'Exit w/ code 77 if system is up to date'

# kargs
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l editor -d 'Use editor to modify kernel args'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l append -d 'Append kernel arg'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l append-if-missing -d 'Append kernel arg if not present'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l delete -d 'Delete kernel arg'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l delete-if-present -d 'Delete kernel arg if present'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l replace -d 'Replace existing kernel arg'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l unchanged-exit-77 -d 'Exit w/ code 77 if kernel args unchanged'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l deploy-index -d 'Use specified index to modify kernel args'
complete -c rpm-ostree -n '__fish_seen_subcommand_from kargs' -l import-proc-cmdline -d 'Use booted kernel args to modify kernel args'

# cleanup
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s p -l pending -d 'Remove pending deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s r -l rollback -d 'Remove default rollback deployment'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s b -l base -d 'Free used space from interrupted ops'
complete -c rpm-ostree -n '__fish_seen_subcommand_from cleanup' -s m -l repomd -d 'Clean up cached RPM repodata and partial downloads'

# initramfs
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l enable -d 'Enable client-side initramfs regen'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l arg -d 'Append custom args to initramfs program'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs' -l disable -d 'Disable initramfs regen'

# initramfs-etc
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l track -d 'Track specified file'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l untrack -d 'Stop tracking files'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l untrack-all -d 'Stop tracking all files'
complete -c rpm-ostree -n '__fish_seen_subcommand_from initramfs-etc' -l force-sync -d 'Generate a new deployment w/out upgrading'

# apply-live
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l reset -d 'Reset filesystem tree to booted commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l target -d 'Target named OSTree commit'
complete -c rpm-ostree -n '__fish_seen_subcommand_from apply-live' -l allow-replacement -d 'Enable live update/remove of extant packages'
