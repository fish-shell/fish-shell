#
# completion for rpm-ostree
#

# Subcommands for rpm-ostree
set -l subcommands apply-live cancel cleanup compose db deploy initramfs initramfs-etc install kargs override rebase refresh-md reload remove reset rollback status uninstall update upgrade usroverlay

# Options for rpm-ostree, most of them also apply to subcommands
set -l options --version --quiet -q --help -h

# subcommands that does not support --quiet
set -l no_quiet apply-live container-encapsulate image usroverlay

# subcommands that does not support --version
set -l no_version apply-live container-encapsulate image

# File completions also need to be disabled
complete -c rpm-ostree -f

# All the options
# --help for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $options" -l help -xs h -d "Show help options"

# --quiet for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from options $options $no_quiet" -l quiet -s q -d "Print minimal informational messages"

# --version for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $options $no_version" -l version -d "Print version info and exit"

# All the subcommands
# There is not --version or --quiet for apply-live
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands --version --quiet -q" -a apply-live -d "Apply pending changes to booted deployment"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a cancel -d "Cancel an active transaction"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a cleanup -d "Clear cached/pending data"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a compose -d "Commands to compose a tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a db -d "Commands to query the RPM database"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a deploy -d "Deploy a specific commit"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a initramfs -d "Toggle local initramfs regeneration"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a initramfs-etc -d "Add files to the initramfs"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a install -d "Overlay additional packages"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a kargs -d "Query or modify kernel arguments"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a override -d "Manage base package overrides"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a rebase -d "Switch to a different tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a refresh-md -d "Generate rpm repo metadata"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a reload -d "Reload configuration"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a reset -d "Remove all mutations"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a rollback -d "Revert to previously booted tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a status -d "Get booted system version"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a uninstall -d "Remove overlayed additional packages"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a remove -d "Remove overlayed additional packages"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a upgrade -d "Perform system upgrade"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a update -d "Alias for upgrade"
# 2023-07-21: Any flag between rpm-ostree and usroverlay can cause Segmentation fault. https://github.com/coreos/rpm-ostree/issues/4508
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands -" -a usroverlay -d "Apply transient overlayfs to /usr"

# apply-live
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l target -d "Target provided commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l reset -d "Reset back to booted commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l allow-replacement -d "Allow replacement of packages/files"

# cancel
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l peer -d "Force peer-to-peer connection"

# cleanup
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l base -s b -d "Clear temporary files; will not change deployments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l pending -s p -d "Remove pending deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l rollback -s r -d "Remove rollback deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l repomd -s m -d "Delete cached rpm repo metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l peer -d "Force peer-to-peer connection"

# compose
# All compose subcommands
# container-encapsulatedoes, image not have --quiet or --verison
set -l compose_subcommands commit container-encapsulate extensions image install postprocess tree
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a commit -d "Commit target path to an OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands --quiet -q --version" -a container-encapsulate -d "Create Chunked Container image from OSTree Commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a extensions -d "Download Depsolved RPMs for Base OSTree"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands --quiet -q --version" -a image -d "Create reproducible Chunked Image from Treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a install -r -d "Install packages into a target path"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a postprocess -d "Final postprocessing on an installation root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a tree -d "Process a treefile"

# compose commit
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l repo -s r -d "=REPO Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l layer-repo -d "=REPO Path to OSTree repo for ostree-layers & ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l add-metadata-string -d "=KEY=VALUE Append given key=value to metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l add-metadata-from-json -d "=JSON Append given JSON file to ostree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l write-commitid-to -d "=FILE Write composed CommitID to file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l write-composejson-to -d "=FIlE Write compose run info to JSON file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l no-parent -d "Always commit without a parent"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l parent -d "=REV Commit with specific parent revision"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -r -F # This seems to enable path completion

# compose container-encapsulate
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l repo -r -d "=REPO Path to repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l label -xs l -d "Additional labels for container"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l copymeta -d "Propagate OSTree key to container label"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l copymeta-opt -d "Propagate opt OSTree key to container label"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l cmd -d "Matches dockerfile CMD instruction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l max-layers -d "Maximum number of container image layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l format-version -d "0 or 1; encapsulated container format version"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l write-contentmeta-json -r -d "Output content metadata as JSON"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l compare-with-build -d "Compare current OCI layers with another imgref"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l previous-build-manifest -d "Preserve pack structure with prior metadata"

# compose extensions
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l repo -s r -d "=REPO Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l layer-repo -d "=REPO Repo for ostree-layers & ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l output-dir -d "=PATH Path to extensions output dir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l base-rev -d "=REV Base OSTree revision"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l cachedir -d "=CACHEDIR Cached state dir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l rootfs -d "=ROOTFS Path to existing rootfs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -l touch-if-changed -d "=FILE Update mod time on file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extensions" -r -F

# compose image
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l cachedir -d "Set cache directory for packages and data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l authfile -d "Container authentication file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l layer-repo -d "Specify repository for ostree layers and ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l initialize -s i -d "Skip previous image query for first build"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l format -d "Choose format (ociarchive oci registry)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l force-nocache -d "Force a build"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l offline -d "Operate on cached data, no network repo access"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l lockfile -d "Specify JSON-formatted lockfile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l label -xs l -d "Add labels (KEY=VALUE) to the container image"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l touch-if-changed -d "Update file timestamp on changes"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l copy-retry-times -d "Set number of copy retries to remote destination"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -r -F

# compose install
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l repo -s r -d "=REPO Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l layer-repo -d "=REPO Repo for ostree-layers & ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l force-nocache -d "Always create new OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l cache-only -d "Assume cache present, no updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l cachedir -d "=CACHEDIR Cached state dir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l download-only -d "Dry-run but download & import RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l download-only-rpms -d "Dry-run but download RPMs; need --cachedir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l proxy -d "=PROXY HTTP proxy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l dry-run -d "Print transaction and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l print-only -d "Expand includes and print treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l touch-if-changed -d "=FILE Update FILE's mod time if new commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-commit -d "=COMMIT for change detection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-inputhash -d "=DIGEST Use input hash for change detect"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-version -d "=VERSION for auto version numbering"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l workdir -d "=WORKDIR Working directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-write-lockfile-to -d "=FILE Write lockfile to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-lockfile -d "=FILE Read lockfile from FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-lockfile-strict -d "Only allow installing locked packages with --ex-lockfile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -r -F

# compose postprocess
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from postprocess" -l unified-core -d "Use new unified core codepath"

# compose tree
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l repo -s r -d "=REPO Path to OSTree repos"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l layer-repo -d "=REPO Repo for ostree-layers & ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l force-nocache -d "Always create new OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l cache-only -d "Assume cache present, no updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l cachedir -d "=CACHEDIR Specify cached state dir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l download-only -d "Dry-run but download & import RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l download-only-rpms -d "Dry-run but download RPMs; need --cachedir"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l proxy -d "=PROXY HTTP proxy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l dry-run -d "Print transaction and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l print-only -d "Expand includes and print treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l touch-if-changed -d "=FILE Update FILE's mod time if new commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-commit -d "=COMMIT for change detection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-inputhash -d "=DIGEST Use input hash for change detect"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-version -d "=VERSION for auto version numbering"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l workdir -d "=WORKDIR Working directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-write-lockfile-to -d "=FILE Write lockfile to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-lockfile -d "=FILE Read lockfile from FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-lockfile-strict -d "Only allow installing locked packages with --ex-lockfile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l add-metadata-string -d "=KEY=VALUE Append key=value to metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l add-metadata-from-json -d "=JSON Append JSON to OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l write-commitid-to -d "=FILE Write commitid to FILE, not ref"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l write-composejson-to -d "=FILE Write compose run info json to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l no-parent -d "Always Commit without a parent"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l parent -d "=REV Commit with specific parent revision"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -r -F

# db
set -l db_subcommands diff list version
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a diff -d "Show package changes between two commits"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a list -d "List packages within commits"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a version -d "Show rpmdb version of packages within the commits"

# db diff
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l repo -s r -r -d "=REPO Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l format -d "=FORMAT Choose output format (block diff json)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l changelogs -s c -d "Include RPM changelogs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l base -d "Diff against deployments' base"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l advisories -s a -d "Include new advisories"

# db list
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from list" -l repo -s r -r -d "=PATH Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from list" -l advisories -s a -d "Include new advisories"

# db version
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from version" -l repo -s r -r -d "=REPO Path to OSTree repo"

# deploy
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l preview -d "Preview package differences"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l cache-only -s C -d "No latest ostree and RPM download"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l download-only -d "Download latest ostree and RPM data, don't deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l skip-branch-check -d "Don't check if commit belongs on the same branch"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l disallow-downgrade -d "Forbid deployment of older trees"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l unchanged-exit-77 -d "If no new deployment made, exit 77"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l register-driver -d "=DRIVERNAME Register agent as driver for updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l bypass-driver -d "Force deploy even if updates driver is registered"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l uninstall -d "=PKG Remove overlayed additional package"

# initramfs
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l enable -d "Enable regenerating initramfs locally using dracut"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l arg -d "=ARG Append ARG to the dracut arguments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l disable -d "Disable regenerating initramfs locally"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l peer -d "Force peer-to-peer connection"

# initramfs-etc
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l force-sync -d "Deploy a new tree with the latest tracked /etc files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l track -r -d "=FILE Track root /etc file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack -r -d "=FILE Untrack root /etc file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack-all -d "Untrack all root /etc files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l unchanged-exit-77 -d "Exit with 77 if no new deployment made"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l peer -d "Force peer-to-peer connection"

# install, there is also compose install
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l uninstall -d "=PKG Remove overlayed additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l cache-only -s C -d "Skip downloading latest ostree/RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l download-only -d "Download latest ostree & RPM data, skip deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l apply-live -s A -d "Apply changes to pending & live filesystem"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l force-replacefiles -d "Allow package to replace files from others' pkgs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l reboot -s r -d "Initiate a reboot post-operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l dry-run -s n -d "Exit after printing the transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l assumeyes -s y -d "Auto-confirm non-security prompts"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l allow-inactive -d "Allow inactive package requests"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l idempotent -d "No action if pkg already (un)installed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l unchanged-exit-77 -d "If no overlays were changed, exit 77"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l enablerepo -d "Enable repo based on its ID"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l disablerepo -d "Only disabling all (*) repo is supported"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l releasever -d "Set the releasever"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install; and not __fish_seen_subcommand_from compose" -l peer -d "Force peer-to-peer connection"

# kargs
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l os -d "=OSNAME Operation on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l deploy-index -d "=INDEX Modify kernel args of a deployment by index"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l reboot -d "Initiate a reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append -d "=KEY=VALUE Append kernel argument"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l replace -d "=KEY=VALUE=NEWVALUE Replace kernel argument"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete -d "=KEY=VALUE Delete kernel argument pair"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append-if-missing -d "=KEY=VALUE Append kernel arg if missing"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete-if-present -d "=KEY=VALUE Delete kernel arg if present"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l unchanged-exit-77 -d "Exit 77 if no kernel args changed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l import-proc-cmdline -d "Modify args from the booted deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l editor -d "Use editor to modify kernel arguments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l peer -d "Force peer-to-peer connection"

# override
set -l override_subcommands remove replace reset
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a remove -d "Remove packages from the base layer"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a replace -d "Replace packages in the base layer"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a reset -d "Reset active package overrides"

# override remove
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l replace -d "=RPM Replace a package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l reboot -s r -d "Initiate a reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l uninstall -d "=PKG Remove overlayed package"

# override replace
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l remove -d "=PKG Remove a package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l reboot -d "Initiate a reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l uninstall -d "=PKG Remove overlayed package"

# override reset
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l all -s a -d "Reset all active overrides"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l reboot -s r -d "Initiate a reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l sysroot -r -d "=SYSROOT Use system root "
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l uninstall -d "=PKG Remove overlayed package"

# rebase
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l branch -s b -x -d "=BRANCH Rebase to branch BRANCH"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l remote -s m -x -d "=REMOTE Rebase to current branch name using REMOTE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l reboot -s r -d "Initiate a reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l skip-purge -d "Keep previous refspec after rebase"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l cache-only -s C -d "Don't download latest ostree/RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l download-only -d "Download ostree & RPM data, don't deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l custom-origin-description -d "Human-readable description of custom origin"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l custom-origin-url -d "Machine-readable description of custom origin"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l experimental -d "Enable experimental features"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l disallow-downgrade -d "Forbid older trees deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l bypass-driver -d "Force rebase despite updates driver is registered"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l uninstall -d "=PKG Remove overlayed package"

# refresh-md
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l force -d "Expire current cache"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l peer -d "Force peer-to-peer connection"

# reload
complete -c rpm-ostree -n "__fish_seen_subcommand_from reload" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l peer -d "Force peer-to-peer connection"

# reset, there is also override reset
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l reboot -s r -d "Reboot after transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l overlays -s l -d "Remove all overlayed packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l overrides -s o -d "Remove all overrides"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l initramfs -s i -d "Stop regenerating initramfs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset; and not __fish_seen_subcommand_from override" -l uninstall -d "=PKG Remove overlayed package"

# rollback
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l reboot -s r -d "Reboot after transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l peer -d "Force peer-to-peer connection"

# status
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l verbose -s v -d "Print additional fields"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l advisories -s a -d "Expand advisories listing"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l json -d "Output JSON"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l jsonpath -s J -d "=EXPRESSION Filter JSONPath expression"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l booted -s b -d "Only print booted deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l pending-exit-77 -d "If pending deployment available, exit 77"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l peer -d "Force peer-to-peer connection"

# uninstall and remove, there is also override remove
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l all -d "Remove all overlayed packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l reboot -s r -d "Reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l dry-run -s n -d "Print transaction, don't execute"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l assumeyes -s y -d "Auto-confirm non-security prompts"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l allow-inactive -d "Allow inactive package requests"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l idempotent -d "Skip if pkg already (un)installed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l unchanged-exit-77 -d "Exit 77 if no overlays changed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l enablerepo -d "Enable repo by id"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l disablerepo -d "Disable all (*) repositories"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l releasever -d "Set the release version"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall remove; and not __fish_seen_subcommand_from override" -l peer -d "Force peer-to-peer connection"

# upgrade
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l os -d "=OSNAME Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l reboot -s r -d "Reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l allow-downgrade -d "Allow older trees deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l preview -d "Preview pkg differences"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l check -d "Check upgrade availability"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l cache-only -s C -d "Don't download OSTree & RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l download-only -d "Download OSTree & RPM data, don't deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l unchanged-exit-77 -d "Exit 77 if no deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l bypass-driver -d "Force upgrade even if updates driver is registered"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l sysroot -r -d "=SYSROOT Use system root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l install -d "=PKG Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from update upgrade" -l uninstall -d "=PKG Remove overlayed package"

# usroverlay
# Usage: rpm-ostree usroverlay
#
# Options:
#   -h, --help     Print help (see more with '--help')
#   -V, --version  Print version
