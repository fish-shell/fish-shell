# Define subcommands for rpm-ostree
set -l subcommands apply-live compose cancel cleanup db deploy initramfs initramfs-etc install kargs override refresh-md reload rebase reset rollback status uninstall upgrade usroverlay

# File completions also need to be disabled
complete -c rpm-ostree -f

# Help options for all rpm-ostree and its subcommands
complete -c rpm-ostree -xl help -xs h -d "Show help options"

# Define auto-completion options for rpm-ostree
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a "$subcommands"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -l version -d "Print version information and exit"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -l quiet -s q -d "Avoid printing most informational messages"

# apply-live
complete -c rpm-ostree -n __fish_use_subcommand -xa apply-live -d "Apply pending deployment changes"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l reset -d "Reset back to booted commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l "target <TARGET>" -d "Target a OSTree commit instead of pending deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l allow-replacement -d "Allow replacement of packages/files"

# cancel
complete -c rpm-ostree -n __fish_use_subcommand -xa cancel -d "Cancel an active transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l sysroot=SYSROOT -d "Use system root SYSROOT (default: /)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l peer -d "Force a peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l version -d "Print version information and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l quiet -s q -d "Avoid printing most informational messages"

# cleanup
complete -c rpm-ostree -n __fish_use_subcommand -xa cleanup -d "Clear cached/pending data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l pending -s p  -d "Remove pending deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l rollback -s r  -d "Remove rollback deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l base -s b  -d "Clear temporary files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l repomd -s m  -d "Delete cached rpm repo metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l sysroot=SYSROOT -d "Use system root SYSROOT (default: /)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l peer -d "Force a peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l version -d "Print version information and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l quiet -s q -d "Avoid printing most informational messages"

# compose
complete -c rpm-ostree -n __fish_use_subcommand -xa compose -d "Commands to compose a tree"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose" -l version -d "Print version information and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose" -l quiet -s q -d "Avoid printing most informational messages"
set -l compose_subcommands commit container-encapsulate extensions image install postprocess tree
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose" -ra commit -d "Commit a target path to an OSTree repository"

# compose - commit





# db
complete -c rpm-ostree -n __fish_use_subcommand -xa db -d "Commands to query the RPM database"

# deploy
complete -c rpm-ostree -n __fish_use_subcommand -xa deploy -d "Deploy a specific commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l unchanged-exit-77 -d "Exit w/ code 77 if system already on specified commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -s r -l reboot -d "Reboot after upgrade is prepared"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l preview -d "Download enough metadata to diff RPM w/out deploying"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -s C -l cache-only -d "Perform operation w/out updating from remotes"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l download-only -d "Download targeted ostree/layered RPMs w/out deploying"

# initramfs
complete -c rpm-ostree -n __fish_use_subcommand -xa initramfs -d "Enable or disable local initramfs regeneration"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l enable -d "Enable client-side initramfs regen"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l arg -d "Append custom args to initramfs program"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l disable -d "Disable initramfs regen"

# initramfs-etc
complete -c rpm-ostree -n __fish_use_subcommand -xa initramfs-etc -d "Add files to the initramfs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l track -d "Track specified file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack -d "Stop tracking files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack-all -d "Stop tracking all files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l force-sync -d "Generate a new deployment w/out upgrading"

# install
complete -c rpm-ostree -n __fish_use_subcommand -xa install -d "Overlay additional packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l idempotent -d "Don't error if package is already present"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -s r -l reboot -d "Reboot after deployment is prepared"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -s n -l dry-run -d "Exit after printing transaction (don't download and deploy)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l allow-inactive -d "Allow packages already in base layer"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -s C -l cache-only -d "Don't download latest packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l download-only -d "Download targeted layered RPMs w/out deploying"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l apply-live -d "Apply changes to booted deployment"

# kargs
complete -c rpm-ostree -n __fish_use_subcommand -xa kargs -d "Query or modify kernel arguments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l editor -d "Use editor to modify kernel args"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append -d "Append kernel arg"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append-if-missing -d "Append kernel arg if not present"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete -d "Delete kernel arg"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete-if-present -d "Delete kernel arg if present"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l replace -d "Replace existing kernel arg"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l unchanged-exit-77 -d "Exit w/ code 77 if kernel args unchanged"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l deploy-index -d "Use specified index to modify kernel args"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l import-proc-cmdline -d "Use booted kernel args to modify kernel args"

# override
complete -c rpm-ostree -n __fish_use_subcommand -xa override -d "Manage base package overrides"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override" -l version -d "Print version info and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override" -l help -s h -d "Show help options"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override" -l quiet -s q -d "Avoid printing most informational message"

# rebase
complete -c rpm-ostree -n __fish_use_subcommand -xa rebase -d "Switch to a different tree"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l branch -d "Pick a branch name"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l remote -d "Pick a remote name"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -s C -l cache-only -d "Perform rebase w/out downloading latest"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l download-only -d "Download targeted ostree/layered RPMs w/out deploying"

# refresh-md
complete -c rpm-ostree -n __fish_use_subcommand -xa refresh-md -d "Generate rpm repo metadata"

# reload
complete -c rpm-ostree -n __fish_use_subcommand -xa reload -d "Reload configuration"

# reset
complete -c rpm-ostree -n __fish_use_subcommand -xa reset -d "Remove all mutations"

# rollback
complete -c rpm-ostree -n __fish_use_subcommand -xa rollback -d "Revert to the previously booted tree"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -s r -l reboot -d "Reboot after rollback is prepared"

# status
complete -c rpm-ostree -n __fish_use_subcommand -xa status -d "Get the version of the booted system"

# uninstall
complete -c rpm-ostree -n __fish_use_subcommand -xa uninstall -d "Remove overlayed additional packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -s r -l reboot -d "Reboot after deployment is prepared"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -s n -l dry-run -d "Exit after printing transaction (don't download and deploy)"

# upgrade and update
complete -c rpm-ostree -n __fish_use_subcommand -xa upgrade -d "Perform a system upgrade"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l allow-downgrade -d "Permit deployment of older trees"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l preview -d "Minimal download in order to do a package-level version diff"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l check -d "Check if upgrade is available w/out downloading it"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -s C -l cache-only -d "Upgrade w/out updating to latest"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l download-only -d "Download targeted ostree/layered RPMs w/out deploying"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -s r -l reboot -d "Reboot after upgrade is prepared"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l unchanged-exit-77 -d "Exit w/ code 77 if system is up to date"

# usroverlay
complete -c rpm-ostree -n __fish_use_subcommand -xa usroverlay -d "Apply a transient overlayfs to /usr"