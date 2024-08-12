# Completions for apk (Alpine Package Keeper)

# Package name
complete -c apk -n "__fish_seen_subcommand_from add" -a "(apk search -q 2>/dev/null)" -d Package
complete -c apk -n "__fish_seen_subcommand_from manifest" -a "(apk info 2>/dev/null)" -d Package
complete -f -c apk -n "__fish_seen_subcommand_from info fetch dot" -a "(apk search -q 2>/dev/null)" -d Package
complete -f -c apk -n "__fish_seen_subcommand_from del fix version" -a "(apk info 2>/dev/null)" -d Package

# Global options
complete -f -c apk -s h -l help -d "Show help"
complete -r -c apk -s p -l root -d "Install packages to DIR"
complete -r -c apk -s X -l repository -d "Use packages from REPO"
complete -f -c apk -s q -l quiet -d "Print less information"
complete -f -c apk -s v -l verbose -d "Print more information"
complete -f -c apk -s i -l interactive -d "Ask confirmation for certain operations"
complete -f -c apk -s V -l version -d "Print program version"
complete -f -c apk -l force-binary-stdout -d "Continue even if binary data is to be output"
complete -f -c apk -l force-broken-world -d "Continue even if 'world' cannot be satisfied"
complete -f -c apk -l force-non-repository -d "Continue even if packages may be lost on reboot"
complete -f -c apk -l force-old-apk -d "Continue even if packages use unsupported features"
complete -f -c apk -l force-overwrite -d "Overwrite files in other packages"
complete -f -c apk -l force-refresh -d "Don't use cached files"
complete -f -c apk -s U -l update-cache -d "Alias for --cache-max-age 1"
complete -f -c apk -l progress -d "Show a progress bar"
complete -x -c apk -l progress-fd -d "Write progress to FD"
complete -f -c apk -l no-progress -d "Disable progress bar even for TTYs"
complete -f -c apk -l purge -d "Delete also modified configuration files and uninstalled packages from cache"
complete -f -c apk -l allow-untrusted -d "Install packages with untrusted signature or no signature"
complete -x -c apk -l wait -d "Wait for TIME seconds to get an exclusive repository lock before failing"
complete -r -c apk -l keys-dir -d "Override directory of trusted keys"
complete -r -c apk -l repositories-file -d "Override repositories file"
complete -f -c apk -l no-network -d "Don't use network"
complete -f -c apk -l no-cache -d "Don't use any local cache path"
complete -r -c apk -l cache-dir -d "Override cache directory"
complete -x -c apk -l cache-max-age -d "Maximum AGE for index in cache before refresh"
complete -x -c apk -l arch -d "Use architecture with --root"
complete -f -c apk -l print-arch -d "Print default architecture"

# Commands
complete -f -c apk -n __fish_use_subcommand -a add -d "Add packages"
complete -f -c apk -n __fish_use_subcommand -a del -d "Remove packages"
complete -f -c apk -n __fish_use_subcommand -a fix -d "Repair package"
complete -f -c apk -n __fish_use_subcommand -a update -d "Update repository indexes"
complete -f -c apk -n __fish_use_subcommand -a info -d "Give detailed information about packages"
complete -f -c apk -n __fish_use_subcommand -a list -d "List packages"
complete -f -c apk -n __fish_use_subcommand -a search -d "Search package"
complete -f -c apk -n __fish_use_subcommand -a upgrade -d "Upgrade installed packages"
complete -f -c apk -n __fish_use_subcommand -a cache -d "Manage local package cache"
complete -f -c apk -n __fish_use_subcommand -a version -d "Compare package versions"
complete -f -c apk -n __fish_use_subcommand -a index -d "Create repository index file"
complete -f -c apk -n __fish_use_subcommand -a fetch -d "Download packages"
complete -f -c apk -n __fish_use_subcommand -a audit -d "Audit the directories for changes"
complete -f -c apk -n __fish_use_subcommand -a verify -d "Verify package integrity and signature"
complete -f -c apk -n __fish_use_subcommand -a dot -d "Generate graphviz graphs"
complete -f -c apk -n __fish_use_subcommand -a policy -d "Show repository policy for packages"
complete -f -c apk -n __fish_use_subcommand -a stats -d "Show statistics about repositories and installations"
complete -f -c apk -n __fish_use_subcommand -a manifest -d "Show checksums of package contents"

# Commit options
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -s s -l simulate -d "Simulate the requested operation"
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -l clean-protected -d "Don't create .apk-new files"
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -l overlay-from-stdin -d "Read list of overlay files from stdin"
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -l no-scripts -d "Don't execute any scripts"
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -l no-commit-hooks -d "Skip pre/post hook scripts"
complete -f -c apk -n "__fish_seen_subcommand_from add del fix upgrade" -l initramfs-diskless-boot -d "Enables options for diskless initramfs boot"

# Add options
complete -f -c apk -n "__fish_seen_subcommand_from add" -l initdb -d "Initialize database"
complete -f -c apk -n "__fish_seen_subcommand_from add" -s u -l upgrade -d "Prefer to upgrade package"
complete -f -c apk -n "__fish_seen_subcommand_from add" -s l -l latest -d "Select latest version of package"
complete -x -c apk -n "__fish_seen_subcommand_from add" -s t -l virtual -d "Create virtual package"
complete -f -c apk -n "__fish_seen_subcommand_from add" -l no-chown -d "Don't change file owner or group"

# Delete options
complete -f -c apk -n "__fish_seen_subcommand_from del" -s r -l rdepends -d "Remove unneeded dependencies too"

# Fix options
complete -f -c apk -n "__fish_seen_subcommand_from fix" -s d -l depends -d "Fix all dependencies too"
complete -f -c apk -n "__fish_seen_subcommand_from fix" -s r -l reinstall -d "Reinstall the package"
complete -f -c apk -n "__fish_seen_subcommand_from fix" -s u -l upgrade -d "Prefer to upgrade package"
complete -f -c apk -n "__fish_seen_subcommand_from fix" -s x -l xattr -d "Fix packages with broken xattrs"
complete -f -c apk -n "__fish_seen_subcommand_from fix" -l directory-permissions -d "Reset all directory permissions"

# Info options
complete -f -c apk -n "__fish_seen_subcommand_from info" -s L -l contents -d "List included files"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s e -l installed -d "Check PACKAGE installed status"
complete -r -c apk -n "__fish_seen_subcommand_from info" -s W -l who-owns -d "Print who owns the file"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s R -l depends -d "List the dependencies"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s P -l provides -d "List virtual packages provided"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s r -l rdepends -d "List reverse dependencies"
complete -f -c apk -n "__fish_seen_subcommand_from info" -l replaces -d "List packages that PACKAGE might replace"
complete -f -c apk -n "__fish_seen_subcommand_from info" -l install-if -d "List install_if rule"
complete -f -c apk -n "__fish_seen_subcommand_from info" -l rinstall-if -d "List packages having install_if referencing PACKAGE"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s w -l webpage -d "Print the URL for the upstream"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s s -l size -d "Show installed size"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s d -l description -d "Print the description"
complete -f -c apk -n "__fish_seen_subcommand_from info" -l license -d "Print the license"
complete -f -c apk -n "__fish_seen_subcommand_from info" -l triggers -d "Print active triggers"
complete -f -c apk -n "__fish_seen_subcommand_from info" -s a -l all -d "Print all information"

# List options
complete -f -c apk -n "__fish_seen_subcommand_from list" -s I -l installed -d "List installed packages only"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s O -l orphaned -d "List orphaned packages only"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s a -l available -d "List available packages only"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s u -l upgradable -d "List upgradable packages only"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s o -l origin -d "List packages by origin"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s d -l depends -d "List packages by dependency"
complete -f -c apk -n "__fish_seen_subcommand_from list" -s P -l providers -d "List packages by provider"

# Search options
complete -f -c apk -n "__fish_seen_subcommand_from search" -s a -l all -d "Show all package versions"
complete -f -c apk -n "__fish_seen_subcommand_from search" -s d -l description -d "Search package descriptions"
complete -f -c apk -n "__fish_seen_subcommand_from search" -s x -l exact -d "Require exact match"
complete -f -c apk -n "__fish_seen_subcommand_from search" -s o -l origin -d "Print origin package name"
complete -f -c apk -n "__fish_seen_subcommand_from search" -s r -l rdepends -d "Print reverse dependencies"
complete -f -c apk -n "__fish_seen_subcommand_from search" -l has-origin -d "List packages that have the given origin"

# Upgrade options
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -s a -l available -d "Reset all packages to the provided versions"
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -s l -l latest -d "Select latest version of package"
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -l no-self-upgrade -d "Don't do early upgrade of the apk"
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -l self-upgrade-only -d "Only do self-upgrade"
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -l ignore -d "Ignore the upgrade of PACKAGE"
complete -f -c apk -n "__fish_seen_subcommand_from upgrade" -l prune -d "Prune the WORLD by removing packages which are no longer available"

# Cache options
complete -f -c apk -n "__fish_seen_subcommand_from cache" -s u -l upgrade -d "Prefer to upgrade package"
complete -f -c apk -n "__fish_seen_subcommand_from cache" -s l -l latest -d "Select latest version of package"

# Version options
complete -f -c apk -n "__fish_seen_subcommand_from version" -s I -l indexes -d "Print description and versions of indexes"
complete -f -c apk -n "__fish_seen_subcommand_from version" -s t -l test -d "Compare two given versions"
complete -f -c apk -n "__fish_seen_subcommand_from version" -s c -l check -d "Check the given version strings"
complete -f -c apk -n "__fish_seen_subcommand_from version" -s a -l all -d "Consider packages from all repository tags"
complete -x -c apk -n "__fish_seen_subcommand_from version" -s l -l limit -d "Limit to packages with output matching given operand"

# Index options
complete -r -c apk -n "__fish_seen_subcommand_from index" -s o -l output -d "Write the generated index to FILE"
complete -r -c apk -n "__fish_seen_subcommand_from index" -s x -l index -d "Read an existing index from INDEX"
complete -x -c apk -n "__fish_seen_subcommand_from index" -s d -l description -d "Add a description to the index"
complete -x -c apk -n "__fish_seen_subcommand_from index" -l rewrite-arch -d "Use ARCH as architecture for all packages"
complete -f -c apk -n "__fish_seen_subcommand_from index" -l no-warnings -d "Disable the warning about missing dependencies"

# Fetch options
complete -f -c apk -n "__fish_seen_subcommand_from fetch" -s L -l link -d "Create hard links"
complete -f -c apk -n "__fish_seen_subcommand_from fetch" -s R -l recursive -d "Fetch all dependencies too"
complete -f -c apk -n "__fish_seen_subcommand_from fetch" -l simulate -d "Simulate the requested operation"
complete -f -c apk -n "__fish_seen_subcommand_from fetch" -s s -l stdout -d "Dump the .apk to stdout"
complete -r -c apk -n "__fish_seen_subcommand_from fetch" -s o -l output -d "Write the downloaded files to DIR"

# Audit options
complete -f -c apk -n "__fish_seen_subcommand_from audit" -l backup -d "Audit configuration files only"
complete -f -c apk -n "__fish_seen_subcommand_from audit" -l system -d "Audit all system files"
complete -f -c apk -n "__fish_seen_subcommand_from audit" -l check-permissions -d "Check file permissions too"
complete -f -c apk -n "__fish_seen_subcommand_from audit" -s r -l recursive -d "Descend into directories and audit them as well"
complete -f -c apk -n "__fish_seen_subcommand_from audit" -l packages -d "List only the changed packages"

# Dot options
complete -f -c apk -n "__fish_seen_subcommand_from dot" -l errors -d "Consider only packages with errors"
complete -f -c apk -n "__fish_seen_subcommand_from dot" -l installed -d "Consider only installed packages"
