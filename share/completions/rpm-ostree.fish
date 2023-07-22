#
# completion for rpm-ostree
#

# Subcommands for rpm-ostree
set -l subcommands apply-live cancel cleanup compose db deploy initramfs initramfs-etc install kargs override refresh-md reload reset rollback status uninstall upgrade usroverlay

# Options for rpm-ostree, most of them also apply to subcommands
set -l options --version --quiet -q --help -h

# subcommands that does not support --quiet
set -l no_quiet apply-live usroverlay image

# subcommands that does not support --version
set -l no_version apply-live image

# File completions also need to be disabled
complete -c rpm-ostree -f

# All the options
# --help for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $options" -xl help -xs h -d "Show help options"

# --quiet for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from options $options $no_quiet" -xl quiet -xs q -d "Print minimal informational messages"

# --version for all
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $options $no_version" -xl version -d "Print version information and exit"

# All the subcommands
# There is not --version or --quiet for apply-live
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands --version --quiet -q" -a apply-live -d "Apply changes to booted deployment"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a cancel -d "Cancel active transaction"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a cleanup -d "Clear cached/pending data"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a compose -d "Commands to compose a tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a db -d "Commands to query the RPM database"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a deploy -d "Deploy specific commit"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a initramfs -d "Toggle local initramfs regeneration"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a initramfs-etc -d "Add files to the initramfs"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a install -d "Overlay additional packages"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a kargs -d "Query or modify kernel arguments"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a override -d "Manage base package overrides"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a rebase -d "Switch to different tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a refresh-md -d "Generate rpm repo metadata"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a reload -d "Reload configuration"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a reset -d "Remove all mutations"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a rollback -d "Revert to previously booted tree"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a status -d "Get booted system version"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a uninstall -d "Remove overlayed additional packages"
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands" -a upgrade -d "Perform system upgrade"
# 2023-07-21: Any flag between rpm-ostree and usroverlay causes Segmentation fault. There is not --quiet flag for usroverlay
complete -c rpm-ostree -n "not __fish_seen_subcommand_from $subcommands $options" -a usroverlay -d "Apply transient overlayfs to /usr"


# apply-live
# Usage: rpm-ostree [OPTIONS]
#
# Options:
#       --target <TARGET>    Target provided commit instead of pending deployment
#       --reset              Reset back to booted commit
#       --allow-replacement  Allow replacement of packages/files (default is pure additive)
#   -h, --help               Print help
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l reset -d "Reset back to booted commit"
# --target is not implemented because this is an advanced feature mainly for testing.
# complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -xl target -d "Target a OSTree commit instead of pending deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from apply-live" -l allow-replacement -d "Allow replacement of packages/files"


# cancel
# Usage:
#   rpm-ostree cancel [OPTION…]
#
# Cancel an active transaction
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cancel" -l peer -d "Force P2P connection"


# cleanup
# Usage:
#   rpm-ostree cleanup [OPTION…]
#
# Clear cached/pending data
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --os=OSNAME           Operate on provided OSNAME
#   -b, --base            Clear temporary files; will leave deployments unchanged
#   -p, --pending         Remove pending deployment
#   -r, --rollback        Remove rollback deployment
#   -m, --repomd          Delete cached rpm repo metadata
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l base -s b -d "Clear temporary files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l pending -s p -d "Remove pending deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l rollback -s r -d "Remove rollback deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l repomd -s m -d "Delete cached rpm repo metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from cleanup" -l peer -d "Force P2P connection"


# compose
# Usage:
#   rpm-ostree compose [OPTION…] COMMAND
#
# Commands to compose a tree
#
# Builtin "compose" Commands:
#   commit           Commit a target path to an OSTree repository
#   container-encapsulateGenerate a reproducible "chunked" container image (using RPM data) from an OSTree commit
#   extensions       Download RPM packages guaranteed to depsolve with a base OSTree
#   image            Generate a reproducible "chunked" container image (using RPM data) from a treefile
#   install          Install packages into a target path
#   postprocess      Perform final postprocessing on an installation root
#   tree             Process a "treefile"; install packages and commit the result to an OSTree repository
#
# Help Options:
#   -h, --help       Show help options
#
# Application Options:
#   --version        Print version information and exit
#   -q, --quiet      Avoid printing most informational messages
# All compose subcommands
# container-encapsulatedoes, image not have --quiet or --verison
set -l compose_subcommands commit container-encapsulate extensions image install postprocess tree
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a commit -d "Commit target path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands --quiet -q --version" -a container-encapsulate -d "Create Chunked Container from OSTree Commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a extensions -d "Download Depsolved RPMs for Base OSTree"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands --quiet -q --version" -a image -d "Create Chunked Image from Treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a install -d "Install packages into a target path"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a postprocess -d "Final postprocessing on an installation root"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and not __fish_seen_subcommand_from $compose_subcommands" -a tree -d "Process a treefile"

# compose commit
# Usage:
#   rpm-ostree compose commit [OPTION…] TREEFILE ROOTFS
#
# Commit a target path to an OSTree repository
#
# Help Options:
#   -h, --help                          Show help options
#
# Application Options:
#   --unified-core                      Use new "unified core" codepath
#   -r, --repo=REPO                     Path to OSTree repository
#   --layer-repo=REPO                   Path to OSTree repository for ostree-layers and ostree-override-layers
#   --add-metadata-string=KEY=VALUE     Append given key and value (in string format) to metadata
#   --add-metadata-from-json=JSON       Parse the given JSON file as object, convert to GVariant, append to OSTree commit
#   --write-commitid-to=FILE            File to write the composed commitid to instead of updating the ref
#   --write-composejson-to=FILE         Write JSON to FILE containing information about the compose run
#   --no-parent                         Always commit without a parent
#   --parent=REV                        Commit with specific parent
#   --version                           Print version information and exit
#   -q, --quiet                         Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l repo=REPO -xs r -d "Path to OSTree repo"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l layer-repo -d "Path to OSTree repo for ostree-layers & ostree-override-layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l add-metadata-string=KEY=VALUE -d "Append given key and value to metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l add-metadata-from-json=JSON -d "Append given JSON to metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l write-commitid-to=FILE -d "Write composed CommitID to file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l write-composejson-to=FILE -d "Write compose run info to JSON"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l no-parent -d "Always commit without a parent"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from commit" -l parent=REV -d "Commit with specific parent"

# compose container-encapsulate
# Usage: container-encapsulate [OPTIONS] --repo <REPO> <OSTREE_REF> <IMGREF>
#
# Arguments:
#   <OSTREE_REF>  OSTree branch name or checksum
#   <IMGREF>      Image reference, e.g. registry:quay.io/exampleos/exampleos:latest
#
# Options:
#       --repo <REPO>
#
#   -l, --label <label>
#           Additional labels for the container
#       --copymeta <copymeta>
#           Propagate an OSTree commit metadata key to container label
#       --copymeta-opt <copymeta-opt>
#           Propagate an optionally-present OSTree commit metadata key to container label
#       --cmd <CMD>
#           Corresponds to the Dockerfile `CMD` instruction
#       --max-layers <MAX_LAYERS>
#           Maximum number of container image layers
#       --format-version <FORMAT_VERSION>
#           The encapsulated container format version; must be 0 or 1 [default: 1]
#       --write-contentmeta-json <WRITE_CONTENTMETA_JSON>
#           Output content metadata as JSON
#       --compare-with-build <compare-with-build>
#           Compare OCI layers of current build with another(imgref)
#       --previous-build-manifest <PREVIOUS_BUILD_MANIFEST>
#           Prevent a change in packing structure by taking a previous build metadata (oci config and manifest)
#   -h, --help
#           Print help
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l repo -d REPO
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l label -xs l -d "Additional labels for container"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l copymeta -d "Propagate OSTree key to container label"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l copymeta-opt -d "Propagate opt OSTree key to container label"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l cmd -d "Matches dockerfile CMD instruction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l max-layers -d "Maximum number of container image layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l format-version -d "Container format version (0 or 1)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l write-contentmeta-json -d "Output content metadata as JSON"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l compare-with-build -d "Compare OCI layers with another imgref"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from container-encapsulate" -l previous-build-manifest -d "Preserve pack structure with prior metadata"

# compose extensions
# Usage:
#   rpm-ostree compose extensions [OPTION…] TREEFILE EXTYAML
#
# Download RPM packages guaranteed to depsolve with a base OSTree
#
# Help Options:
#   -h, --help                  Show help options
#
# Application Options:
#   --unified-core              Use new "unified core" codepath
#   -r, --repo=REPO             Path to OSTree repository
#   --layer-repo=REPO           Path to OSTree repository for ostree-layers and ostree-override-layers
#   --output-dir=PATH           Path to extensions output directory
#   --base-rev=REV              Base OSTree revision
#   --cachedir=CACHEDIR         Cached state
#   --rootfs=ROOTFS             Path to already present rootfs
#   --touch-if-changed=FILE     Update the modification time on FILE if new extensions were downloaded
#   --version                   Print version information and exit
#   -q, --quiet                 Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l repo=REPO -xs r=REPO -d "Specify path to OSTree repository"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l layer-repo=REPO -d "Path to repository for ostree layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l output-dir=PATH -d "Specify extensions output directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l base-rev=REV -d "Specify base OSTree revision"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l cachedir=CACHEDIR -d "Specify cached state directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l rootfs=ROOTFS -d "Path to existing rootfs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from extension" -l touch-if-changed=FILE -d "Update mod. time if extensions downloaded"

# compose image
# Usage: baseimage [OPTIONS] <MANIFEST> <OUTPUT>
#
# Arguments:
#   <MANIFEST>  Path to the manifest file
#   <OUTPUT>    Target path to write
#
# Options:
#       --cachedir <CACHEDIR>
#           Directory to use for caching downloaded packages and other data
#       --authfile <AUTHFILE>
#           Container authentication file
#       --layer-repo <LAYER_REPO>
#           OSTree repository to use for `ostree-layers` and `ostree-override-layers`
#   -i, --initialize
#           Do not query previous image in target location; use this for the first build
#       --format <FORMAT>
#           [default: ociarchive] [possible values: ociarchive, oci, registry]
#       --force-nocache
#           Force a build
#       --offline
#           Operate only on cached data, do not access network repositories
#       --lockfile <LOCKFILES>
#           JSON-formatted lockfile; can be specified multiple times
#   -l, --label <label>
#           Additional labels for the container image, in KEY=VALUE format
#       --touch-if-changed <TOUCH_IF_CHANGED>
#           Update the timestamp or create this file on changes
#       --copy-retry-times <COPY_RETRY_TIMES>
#           Number of times to retry copying an image to remote destination (e.g. registry)
#   -h, --help
#           Print help
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l cachedir -d "Set cache directory for packages and data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l authfile -d "Specify container authentication file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l layer-repo -d "Specify repository for ostree layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l initialize -d "Skip previous image query for first build"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l format -d "Choose format (default: ociarchive)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l force-nocache -d "Force a new build"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l offline -d "Operate with cached data, no network access"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l lockfile -d "Specify JSON-formatted lockfile(s)"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l label -d "Add labels to the container image"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l touch-if-changed -d "Update file timestamp on changes"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from image" -l copy-retry-times -d "Set number of copy retries to destination"

# compose install
# Usage:
#   rpm-ostree compose install [OPTION…] TREEFILE DESTDIR
#
# Install packages into a target path
#
# Help Options:
#   -h, --help                      Show help options
#
# Application Options:
#   --unified-core                  Use new "unified core" codepath
#   -r, --repo=REPO                 Path to OSTree repository
#   --layer-repo=REPO               Path to OSTree repository for ostree-layers and ostree-override-layers
#   --force-nocache                 Always create a new OSTree commit, even if nothing appears to have changed
#   --cache-only                    Assume cache is present, do not attempt to update it
#   --cachedir=CACHEDIR             Cached state
#   --download-only                 Like --dry-run, but download and import RPMs as well; requires --cachedir
#   --download-only-rpms            Like --dry-run, but download RPMs as well; requires --cachedir
#   --proxy=PROXY                   HTTP proxy
#   --dry-run                       Just print the transaction and exit
#   --print-only                    Just expand any includes and print treefile
#   --touch-if-changed=FILE         Update the modification time on FILE if a new commit was created
#   --previous-commit=COMMIT        Use this commit for change detection
#   --previous-inputhash=DIGEST     Use this input hash for change detection
#   --previous-version=VERSION      Use this version number for automatic version numbering
#   --workdir=WORKDIR               Working directory
#   --ex-write-lockfile-to=FILE     Write lockfile to FILE
#   --ex-lockfile=FILE              Read lockfile from FILE
#   --ex-lockfile-strict            With --ex-lockfile, only allow installing locked packages
#   --version                       Print version information and exit
#   -q, --quiet                     Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l repo=REPO -xs r -d "Specify path to OSTree repository"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l layer-repo=REPO -d "Set repo for ostree layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l force-nocache -d "Always create new OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l cache-only -d "Assume cache present, no updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l cachedir=CACHEDIR -d "Specify cached state directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l download-only -d "Dry-run plus download RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l download-only-rpms -d "Dry-run plus download RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l proxy=PROXY -d "Specify HTTP proxy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l dry-run -d "Print transaction and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l print-only -d "Expand includes and print treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l touch-if-changed=FILE -d "Update FILE's time if new commit"1
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-commit=COMMIT -d "Use commit for change detection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-inputhash=DIGEST -d "Use input hash for change detect"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l previous-version=VERSION -d "Use version for auto version number"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l workdir=WORKDIR -d "Specify working directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-write-lockfile-to=FILE -d "Write lockfile to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-lockfile=FILE -d "Read lockfile from FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from install" -l ex-lockfile-strict -d "Only allow installing locked packages"

# compose postprocess
# Usage:
#   rpm-ostree compose postprocess [OPTION…] ROOTFS [TREEFILE]
#
# Perform final postprocessing on an installation root
#
# Help Options:
#   -h, --help         Show help options
#
# Application Options:
#   --unified-core     Use new "unified core" codepath
#   --version          Print version information and exit
#   -q, --quiet        Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from postprocess" -l unified-core -d "Use new unified core codepath"

# compose tree
# Usage:
#   rpm-ostree compose tree [OPTION…] TREEFILE
#
# Process a "treefile"; install packages and commit the result to an OSTree repository
#
# Help Options:
#   -h, --help                          Show help options
#
# Application Options:
#   --unified-core                      Use new "unified core" codepath
#   -r, --repo=REPO                     Path to OSTree repository
#   --layer-repo=REPO                   Path to OSTree repository for ostree-layers and ostree-override-layers
#   --force-nocache                     Always create a new OSTree commit, even if nothing appears to have changed
#   --cache-only                        Assume cache is present, do not attempt to update it
#   --cachedir=CACHEDIR                 Cached state
#   --download-only                     Like --dry-run, but download and import RPMs as well; requires --cachedir
#   --download-only-rpms                Like --dry-run, but download RPMs as well; requires --cachedir
#   --proxy=PROXY                       HTTP proxy
#   --dry-run                           Just print the transaction and exit
#   --print-only                        Just expand any includes and print treefile
#   --touch-if-changed=FILE             Update the modification time on FILE if a new commit was created
#   --previous-commit=COMMIT            Use this commit for change detection
#   --previous-inputhash=DIGEST         Use this input hash for change detection
#   --previous-version=VERSION          Use this version number for automatic version numbering
#   --workdir=WORKDIR                   Working directory
#   --ex-write-lockfile-to=FILE         Write lockfile to FILE
#   --ex-lockfile=FILE                  Read lockfile from FILE
#   --ex-lockfile-strict                With --ex-lockfile, only allow installing locked packages
#   --add-metadata-string=KEY=VALUE     Append given key and value (in string format) to metadata
#   --add-metadata-from-json=JSON       Parse the given JSON file as object, convert to GVariant, append to OSTree commit
#   --write-commitid-to=FILE            File to write the composed commitid to instead of updating the ref
#   --write-composejson-to=FILE         Write JSON to FILE containing information about the compose run
#   --no-parent                         Always commit without a parent
#   --parent=REV                        Commit with specific parent
#   --version                           Print version information and exit
#   -q, --quiet                         Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l unified-core -d "Use new unified core codepath"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l repo=REPO -d "Specify path to OSTree repository"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l layer-repo=REPO -d "Set repo for ostree layers"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l force-nocache -d "Always create new OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l cache-only -d "Assume cache present, no updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l cachedir=CACHEDIR -d "Specify cached state directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l download-only -d "Dry-run plus download RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l download-only-rpms -d "Dry-run plus download RPMs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l proxy=PROXY -d "Specify HTTP proxy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l dry-run -d "Print transaction and exit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l print-only -d "Expand includes and print treefile"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l touch-if-changed=FILE -d "Update FILE's time if new commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-commit=COMMIT -d "Use commit for change detection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-inputhash=DIGEST -d "Use input hash for change detect"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l previous-version=VERSION -d "Use version for auto version number"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l workdir=WORKDIR -d "Specify working directory"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-write-lockfile-to=FILE -d "Write lockfile to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-lockfile=FILE -d "Read lockfile from FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l ex-lockfile-strict -d "Only allow installing locked packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l add-metadata-string=KEY=VALUE -d "Append key-value to metadata"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l add-metadata-from-json=JSON -d "Append JSON to OSTree commit"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l write-commitid-to=FILE -d "Write commitid to FILE, not ref"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l write-composejson-to=FILE -d "Write compose run info to FILE"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l no-parent -d "Commit without a parent"
complete -c rpm-ostree -n "__fish_seen_subcommand_from compose; and __fish_seen_subcommand_from tree" -l parent=REV -d "Commit with specific parent"


# db
# Usage:
#   rpm-ostree db [OPTION…] COMMAND
#
# Commands to query the RPM database
#
# Builtin "db" Commands:
#   diff             Show package changes between two commits
#   list             List packages within commits
#   version          Show rpmdb version of packages within the commits
#
# Help Options:
#   -h, --help       Show help options
#
# Application Options:
#   --version        Print version information and exit
#   -q, --quiet      Avoid printing most informational messages
set -l db_subcommands diff list version
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a diff -d "Show package changes between two commits"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a list -d "List packages within commits"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and not __fish_seen_subcommand_from $db_subcommands" -a version -d "Show rpmdb version of packages within the commits"

# db diff
# Usage:
#   rpm-ostree db diff [OPTION…] [FROM_REV] [TO_REV]
#
# Show package changes between two commits
#
# Help Options:
#   -h, --help              Show help options
#
# Application Options:
#   -r, --repo=PATH         Path to OSTree repository (defaults to /sysroot/ostree/repo)
#   -F, --format=FORMAT     Output format: "diff" or "json" or (default) "block"
#   -c, --changelogs        Also output RPM changelogs
#   --sysroot=SYSROOT       Use system root SYSROOT (default: /)
#   --base                  Diff against deployments' base, not layered commits
#   -a, --advisories        Also output new advisories
#   --version               Print version information and exit
#   -q, --quiet             Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l repo=PATH -d "Specify path to OSTree repository"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l format=FORMAT -d "Choose output format"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l changelogs -s c -d "Include RPM changelogs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l base -d "Diff against deployments' base"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from diff" -l advisories -s a -d "Include new advisories"

# db list
# Usage:
#   rpm-ostree db list [OPTION…] REV... [PREFIX-PKGNAME...]
#
# List packages within commits
#
# Help Options:
#   -h, --help           Show help options
#
# Application Options:
#   -r, --repo=PATH      Path to OSTree repository (defaults to /sysroot/ostree/repo)
#   -a, --advisories     Also list advisories
#   --version            Print version information and exit
#   -q, --quiet          Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from list" -l advisories -s a -d "Include new advisories"
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from list" -l repo=PATH -d "Specify path to OSTree repository"

# db version
# Usage:
#   rpm-ostree db version [OPTION…] COMMIT...
#
# Show rpmdb version of packages within the commits
#
# Help Options:
#   -h, --help          Show help options
#
# Application Options:
#   -r, --repo=PATH     Path to OSTree repository (defaults to /sysroot/ostree/repo)
#   --version           Print version information and exit
#   -q, --quiet         Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from db; and __fish_seen_subcommand_from version" -l repo=PATH -d "Specify path to OSTree repository"

# deploy
# Usage:
#   rpm-ostree deploy [OPTION…] REVISION
#
# Deploy a specific commit
#
# Help Options:
#   -h, --help                       Show help options
#
# Application Options:
#   --os=OSNAME                      Operate on provided OSNAME
#   -r, --reboot                     Initiate a reboot after operation is complete
#   --preview                        Just preview package differences
#   -C, --cache-only                 Do not download latest ostree and RPM data
#   --download-only                  Just download latest ostree and RPM data, don't deploy
#   --skip-branch-check              Do not check if commit belongs on the same branch
#   --disallow-downgrade             Forbid deployment of chronologically older trees
#   --unchanged-exit-77              If no new deployment made, exit 77
#   --register-driver=DRIVERNAME     Register the calling agent as the driver for updates; if REVISION is an empty string, register driver without deploying
#   --bypass-driver                  Force a deploy even if an updates driver is registered
#   --sysroot=SYSROOT                Use system root SYSROOT (default: /)
#   --peer                           Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG                    Overlay additional package
#   --uninstall=PKG                  Remove overlayed additional package
#   --version                        Print version information and exit
#   -q, --quiet                      Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l preview -d "Preview package differences"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l cache-only -s C -d "No latest ostree and RPM download"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l download-only -d "Download latest ostree and RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l skip-branch-check -d "Skip commit branch check"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l disallow-downgrade -d "Forbid deployment of older trees"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l unchanged-exit-77 -d "Exit 77 if no new deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l register-driver=DRIVERNAME -d "Register agent as driver for updates"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l bypass-driver -d "Force deploy even if driver registered"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l peer -d "Force P2P connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from deploy" -l uninstall=PKG -d "Remove overlayed additional package"

# initramfs
# Usage:
#   rpm-ostree initramfs [OPTION…]
#
# Enable or disable local initramfs regeneration
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --os=OSNAME           Operate on provided OSNAME
#   --enable              Enable regenerating initramfs locally using dracut
#   --arg=ARG             Append ARG to the dracut arguments
#   --disable             Disable regenerating initramfs locally
#   -r, --reboot          Initiate a reboot after operation is complete
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l enable -d "Enable regenerating initramfs locally using dracut"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l arg=ARG -d "Append ARG to the dracut arguments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l disable -d "Disable regenerating initramfs locally"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs" -l peer -d "Force P2P connection"

# initramfs-etc
# Usage:
#   rpm-ostree initramfs-etc [OPTION…]
#
# Add files to the initramfs
#
# Help Options:
#   -h, --help              Show help options
#
# Application Options:
#   --os=OSNAME             Operate on provided OSNAME
#   --force-sync            Deploy a new tree with the latest tracked /etc files
#   --track=FILE            Track root /etc file
#   --untrack=FILE          Untrack root /etc file
#   --untrack-all           Untrack all root /etc files
#   -r, --reboot            Initiate a reboot after operation is complete
#   --unchanged-exit-77     If no new deployment made, exit 77
#   --sysroot=SYSROOT       Use system root SYSROOT (default: /)
#   --peer                  Force a peer-to-peer connection instead of using the system message bus
#   --version               Print version information and exit
#   -q, --quiet             Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l force-sync -d "Deploy a new tree with the latest tracked /etc files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l track=FILE -d "Track root /etc file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack=FILE -d "Untrack root /etc file"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l untrack-all -d "Untrack all root /etc files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l reboot -s r -d "Reboot after operation complete"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l unchanged-exit-77 -d "Exit with 77 if no new deployment made"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from initramfs-etc" -l peer -d "Force P2P connection"

# install
# Usage:
#   rpm-ostree install [OPTION…] PACKAGE [PACKAGE...]
#
# Overlay additional packages
#
# Help Options:
#   -h, --help               Show help options
#
# Application Options:
#   --uninstall=PKG          Remove overlayed additional package
#   -C, --cache-only         Do not download latest ostree and RPM data
#   --download-only          Just download latest ostree and RPM data, don't deploy
#   -A, --apply-live         Apply changes to both pending deployment and running filesystem tree
#   --force-replacefiles     Allow package to replace files from other packages
#   --os=OSNAME              Operate on provided OSNAME
#   -r, --reboot             Initiate a reboot after operation is complete
#   -n, --dry-run            Exit after printing the transaction
#   -y, --assumeyes          Auto-confirm interactive prompts for non-security questions
#   --allow-inactive         Allow inactive package requests
#   --idempotent             Do nothing if package already (un)installed
#   --unchanged-exit-77      If no overlays were changed, exit 77
#   --enablerepo             Enable the repository based on the repo id
#   --disablerepo            Only disabling all (*) repositories is supported currently
#   --releasever             Set the releasever
#   --sysroot=SYSROOT        Use system root SYSROOT (default: /)
#   --peer                   Force a peer-to-peer connection instead of using the system message bus
#   --version                Print version information and exit
#   -q, --quiet              Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l uninstall=PKG -d "Remove overlayed additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l cache-only -s C -d "Skip downloading latest ostree/RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l download-only -d "Download ostree/RPM data, skip deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l apply-live -s A -d "Apply changes to pending & live filesystem"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l force-replacefiles -d "Permit package to overwrite others' files"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l reboot -s r -d "Initiate reboot post-operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l dry-run -s n -d "Exit after printing the transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l assumeyes -d "Auto-confirm non-security prompts"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l allow-inactive -d "Allow inactive package requests"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l idempotent -d "No action if pkg already (un)installed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l unchanged-exit-77 -d "If no overlays were changed, exit 77"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l enablerepo -d "Enable repo based on its ID"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l disablerepo -d "Only all (*) repo disabling is supported"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l releasever -d "Set the releasever"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from install" -l peer -d "Force P2P connection"

# kargs
# Usage:
#   rpm-ostree kargs [OPTION…]
#
# Query or modify kernel arguments
#
# Help Options:
#   -h, --help                        Show help options
#
# Application Options:
#   --os=OSNAME                       Operation on provided OSNAME
#   --deploy-index=INDEX              Modify the kernel args from a specific deployment based on index. Index is in the form of a number (e.g. 0 means the first deployment in the list)
#   --reboot                          Initiate a reboot after operation is complete
#   --append=KEY=VALUE                Append kernel argument; useful with e.g. console= that can be used multiple times. empty value for an argument is allowed
#   --replace=KEY=VALUE=NEWVALUE      Replace existing kernel argument, the user is also able to replace an argument with KEY=VALUE if only one value exist for that argument
#   --delete=KEY=VALUE                Delete a specific kernel argument key/val pair or an entire argument with a single key/value pair
#   --append-if-missing=KEY=VALUE     Like --append, but does nothing if the key is already present
#   --delete-if-present=KEY=VALUE     Like --delete, but does nothing if the key is already missing
#   --unchanged-exit-77               If no kernel args changed, exit 77
#   --import-proc-cmdline             Instead of modifying old kernel arguments, we modify args from current /proc/cmdline (the booted deployment)
#   --editor                          Use an editor to modify the kernel arguments
#   --sysroot=SYSROOT                 Use system root SYSROOT (default: /)
#   --peer                            Force a peer-to-peer connection instead of using the system message bus
#   --version                         Print version information and exit
#   -q, --quiet                       Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l os=OSNAME -d "Operation on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l deploy-index=INDEX -d "Modify kernel args by index"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l reboot -d "Initiate reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append=KEY=VALUE -d "Append kernel argument"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l replace=KEY=VALUE=NEWVALUE -d "Replace kernel argument"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete=KEY=VALUE -d "Delete kernel argument pair"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l append-if-missing=KEY=VALUE -d "Append kernel arg if missing"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l delete-if-present=KEY=VALUE -d "Delete kernel arg if present"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l unchanged-exit-77 -d "Exit 77 if no kernel args changed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l import-proc-cmdline -d "Modify args from current /proc/cmdline"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l editor -d "Use editor to modify kernel arguments"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from kargs" -l peer -d "Force P2P connection"


# override
# Usage:
#   rpm-ostree override [OPTION…] COMMAND
#
# Manage base package overrides
#
# Builtin "override" Commands:
#   remove           Remove packages from the base layer
#   replace          Replace packages in the base layer
#   reset            Reset currently active package overrides
#
# Help Options:
#   -h, --help       Show help options
#
# Application Options:
#   --version        Print version information and exit
#   -q, --quiet      Avoid printing most informational messages
set -l override_subcommands remove replace reset
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a remove -d "Remove packages from the base layer"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a replace -d "Replace packages in the base layer"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and not __fish_seen_subcommand_from $override_subcommands" -a reset -d "Reset active package overrides"

# override remove
# Usage:
#   rpm-ostree override remove [OPTION…] PACKAGE [PACKAGE...]
#
# Remove packages from the base layer
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --replace=RPM         Replace a package
#   --os=OSNAME           Operate on provided OSNAME
#   -r, --reboot          Initiate a reboot after operation is complete
#   -n, --dry-run         Exit after printing the transaction
#   -C, --cache-only      Only operate on cached data
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG         Overlay additional package
#   --uninstall=PKG       Remove overlayed additional package
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l replace=RPM -d "Replace a package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l reboot -s r -d "Initiate reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from remove" -l uninstall=PKG -d "Remove overlayed package"

# override replace
# Usage:
#   rpm-ostree override replace [OPTION…] PACKAGE [PACKAGE...]
#
# Replace packages in the base layer
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --remove=PKG          Remove a package
#   --os=OSNAME           Operate on provided OSNAME
#   --reboot              Initiate a reboot after operation is complete
#   -n, --dry-run         Exit after printing the transaction
#   -C, --cache-only      Only operate on cached data
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG         Overlay additional package
#   --uninstall=PKG       Remove overlayed additional package
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l remove=PKG -d "Remove a package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l reboot -d "Initiate reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from replace" -l uninstall=PKG -d "Remove overlayed package"

# override reset
# Usage:
#   rpm-ostree override reset [OPTION…] PACKAGE [PACKAGE...]
#
# Reset currently active package overrides
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   -a, --all             Reset all active overrides
#   --os=OSNAME           Operate on provided OSNAME
#   -r, --reboot          Initiate a reboot after operation is complete
#   -n, --dry-run         Exit after printing the transaction
#   -C, --cache-only      Only operate on cached data
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG         Overlay additional package
#   --uninstall=PKG       Remove overlayed additional package
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l all -s a -d "Reset all active overrides"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l reboot -s r -d "Initiate reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l dry-run -s n -d "Exit after printing transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l cache-only -s C -d "Only operate on cached data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from override; and __fish_seen_subcommand_from reset" -l uninstall=PKG -d "Remove overlayed package"

# rebase
# Usage:
#   rpm-ostree rebase [OPTION…] REFSPEC [REVISION]
#
# Switch to a different tree
#
# Help Options:
#   -h, --help                      Show help options
#
# Application Options:
#   --os=OSNAME                     Operate on provided OSNAME
#   -b, --branch=BRANCH             Rebase to branch BRANCH; use --remote to change remote as well
#   -m, --remote=REMOTE             Rebase to current branch name using REMOTE; may also be combined with --branch
#   -r, --reboot                    Initiate a reboot after operation is complete
#   --skip-purge                    Keep previous refspec after rebase
#   -C, --cache-only                Do not download latest ostree and RPM data
#   --download-only                 Just download latest ostree and RPM data, don't deploy
#   --custom-origin-description     Human-readable description of custom origin
#   --custom-origin-url             Machine-readable description of custom origin
#   --experimental                  Enable experimental features
#   --disallow-downgrade            Forbid deployment of chronologically older trees
#   --bypass-driver                 Force a rebase even if an updates driver is registered
#   --sysroot=SYSROOT               Use system root SYSROOT (default: /)
#   --peer                          Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG                   Overlay additional package
#   --uninstall=PKG                 Remove overlayed additional package
#   --version                       Print version information and exit
#   -q, --quiet                     Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l branch=BRANCH -xs b -d "Rebase to branch BRANCH"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l remote=REMOTE -xs m -d "Rebase using REMOTE with current branch"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l reboot -s r -d "Initiate reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l skip-purge -d "Keep previous refspec after rebase"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l cache-only -s C -d "Don't download latest ostree/RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l download-only -d "Download ostree/RPM data, don't deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l custom-origin-description -d "Describe custom origin"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l custom-origin-url -d "URL of custom origin"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l experimental -d "Enable experimental features"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l disallow-downgrade -d "Forbid older trees deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l bypass-driver -d "Force rebase despite updates driver"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l peer -d "Force P2P connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rebase" -l uninstall=PKG -d "Remove overlayed package"


# refresh-md
# Usage:
#   rpm-ostree refresh-md [OPTION…]
#
# Generate rpm repo metadata
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --os=OSNAME           Operate on provided OSNAME
#   -f, --force           Expire current cache
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l force -d "Expire current cache"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l peer -d "Force P2P connection"


# reload
# Usage:
#   rpm-ostree reload [OPTION…]
#
# Reload configuration
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from reload" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from refresh-md" -l peer -d "Force P2P connection"


# reset
# Usage:
#   rpm-ostree reset [OPTION…]
#
# Remove all mutations
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   --os=OSNAME           Operate on provided OSNAME
#   -r, --reboot          Initiate a reboot after transaction is complete
#   -l, --overlays        Remove all overlayed packages
#   -o, --overrides       Remove all overrides
#   -i, --initramfs       Stop regenerating initramfs or tracking files
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG         Overlay additional package
#   --uninstall=PKG       Remove overlayed additional package
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l reboot -s r -d "Reboot after transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l overlays -s l -d "Remove all overlayed packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l overrides -s o -d "Remove all overrides"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l initramfs -s i -d "Stop regenerating initramfs"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l peer -d "Force P2P connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from reset" -l uninstall=PKG -d "Remove overlayed package"



# rollback
# Usage:
#   rpm-ostree rollback [OPTION…]
#
# Revert to the previously booted tree
#
# Help Options:
#   -h, --help            Show help options
#
# Application Options:
#   -r, --reboot          Initiate a reboot after operation is complete
#   --sysroot=SYSROOT     Use system root SYSROOT (default: /)
#   --peer                Force a peer-to-peer connection instead of using the system message bus
#   --version             Print version information and exit
#   -q, --quiet           Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l reboot -s r -d "Reboot after transaction"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from rollback" -l peer -d "Force P2P connection"

# status
# Usage:
#   rpm-ostree status [OPTION…]
#
# Get the version of the booted system
#
# Help Options:
#   -h, --help                    Show help options
#
# Application Options:
#   -v, --verbose                 Print additional fields (e.g. StateRoot); implies -a
#   -a, --advisories              Expand advisories listing
#   --json                        Output JSON
#   -J, --jsonpath=EXPRESSION     Filter JSONPath expression
#   -b, --booted                  Only print the booted deployment
#   --pending-exit-77             If pending deployment available, exit 77
#   --sysroot=SYSROOT             Use system root SYSROOT (default: /)
#   --peer                        Force a peer-to-peer connection instead of using the system message bus
#   --version                     Print version information and exit
#   -q, --quiet                   Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l verbose -s v -d "Print additional fields"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l advisories -s a -d "Expand advisories listing"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l json -d "Output JSON"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l jsonpath=EXPRESSION -s J -d "Filter JSONPath expression"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l booted -s b -d "Only print booted deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l pending-exit-77 -d "If pending deployment available, exit 77"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from status" -l peer -d "Force P2P connection"



# uninstall
# Usage:
#   rpm-ostree uninstall [OPTION…] PACKAGE [PACKAGE...]
#
# Remove overlayed additional packages
#
# Help Options:
#   -h, --help              Show help options
#
# Application Options:
#   --install=PKG           Overlay additional package
#   --all                   Remove all overlayed additional packages
#   --os=OSNAME             Operate on provided OSNAME
#   -r, --reboot            Initiate a reboot after operation is complete
#   -n, --dry-run           Exit after printing the transaction
#   -y, --assumeyes         Auto-confirm interactive prompts for non-security questions
#   --allow-inactive        Allow inactive package requests
#   --idempotent            Do nothing if package already (un)installed
#   --unchanged-exit-77     If no overlays were changed, exit 77
#   --enablerepo            Enable the repository based on the repo id
#   --disablerepo           Only disabling all (*) repositories is supported currently
#   --releasever            Set the releasever
#   --sysroot=SYSROOT       Use system root SYSROOT (default: /)
#   --peer                  Force a peer-to-peer connection instead of using the system message bus
#   --version               Print version information and exit
#   -q, --quiet             Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l all -d "Remove all overlayed packages"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l reboot -s r -d "Reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l dry-run -s n -d "Print transaction, don't execute"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l assumeyes -s y -d "Auto-confirm non-security prompts"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l allow-inactive -d "Allow inactive package requests"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l idempotent -d "Skip if pkg already (un)installed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l unchanged-exit-77 -d "Exit 77 if no overlays changed"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l enablerepo -d "Enable repo by id"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l disablerepo -d "Disable all (*) repositories"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l releasever -d "Set the release version"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from uninstall" -l peer -d "Force P2P connection"


# upgrade
# Usage:
#   rpm-ostree upgrade [OPTION…]
#
# Perform a system upgrade
#
# Help Options:
#   -h, --help              Show help options
#
# Application Options:
#   --os=OSNAME             Operate on provided OSNAME
#   -r, --reboot            Initiate a reboot after operation is complete
#   --allow-downgrade       Permit deployment of chronologically older trees
#   --preview               Just preview package differences (implies --unchanged-exit-77)
#   --check                 Just check if an upgrade is available (implies --unchanged-exit-77)
#   -C, --cache-only        Do not download latest ostree and RPM data
#   --download-only         Just download latest ostree and RPM data, don't deploy
#   --unchanged-exit-77     If no new deployment made, exit 77
#   --bypass-driver         Force an upgrade even if an updates driver is registered
#   --sysroot=SYSROOT       Use system root SYSROOT (default: /)
#   --peer                  Force a peer-to-peer connection instead of using the system message bus
#   --install=PKG           Overlay additional package
#   --uninstall=PKG         Remove overlayed additional package
#   --version               Print version information and exit
#   -q, --quiet             Avoid printing most informational messages
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l os=OSNAME -d "Operate on provided OSNAME"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l reboot -s r -d "Reboot after operation"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l allow-downgrade -d "Allow older trees deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l preview -d "Preview pkg differences"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l check -d "Check upgrade availability"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l cache-only -s C -d "Don't download ostree/RPM data"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l download-only -d "Download data, don't deploy"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l unchanged-exit-77 -d "Exit 77 if no deployment"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l bypass-driver -d "Force upgrade bypassing driver"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l sysroot=SYSROOT -d "Use system root SYSROOT"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l peer -d "Force peer-to-peer connection"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l install=PKG -d "Overlay additional package"
complete -c rpm-ostree -n "__fish_seen_subcommand_from upgrade" -l uninstall=PKG -d "Remove overlayed package"


# usroverlay
# Usage: rpm-ostree usroverlay
#
# Options:
#   -h, --help     Print help (see more with '--help')
#   -V, --version  Print version
