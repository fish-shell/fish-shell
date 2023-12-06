#checkinstall

# Set yes/no variable
set -l yn "yes no"

# Base-level completion
complete -c checkinstall -f

# Package type selection
complete -c checkinstall -x -o t -l type -a "slackware rpm debian" -d "Choose packaging system"
complete -c checkinstall -f -o S -d "Build a Slackware package"
complete -c checkinstall -f -o R -d "Build an RPM package"
complete -c checkinstall -f -o D -d "Build a Debian package"

# Install options
complete -c checkinstall -x -l install -a "$yn" -d "Toggle created package installation"
complete -c checkinstall -x -l fstrans -a "$yn" -d "Enable/disable the filesystem translation code"

# Scripting options
complete -c checkinstall -f -o y -l default -d "Accept default answers to all questions"
complete -c checkinstall -x -l pkgname -d "Set name"
complete -c checkinstall -x -l pkgversion -d "Set version"
complete -c checkinstall -x -o A -l arch -l pkgarch -d "Set architecture"
complete -c checkinstall -x -l pkgrelease -d "Set release"
complete -c checkinstall -x -l pkglicense -d "Set license"
complete -c checkinstall -x -l pkggroup -d "Set software group"
complete -c checkinstall -x -l pkgsource -d "Set source location"
complete -c checkinstall -x -l pkgalitsource -d "Set alternate source location"
complete -c checkinstall -F -r -l pakdir -a "(__fish_complete_directories)" -d "The new package will be saved here"
complete -c checkinstall -x -l maintainer -d "The package maintainer (.deb)"
complete -c checkinstall -x -l provides -d "Features provided by this package"
complete -c checkinstall -x -l requires -d "Features required by this package"
complete -c checkinstall -x -l recommends -d "Features recommended by this package"
complete -c checkinstall -x -l sggests -d "Featues suggested by this package"
complete -c checkinstall -x -l conflicts -d "Packages that this package cannot be installed with (.deb)"
complete -c checkinstall -x -l replaces -d "Packages that this package replaces (.deb)"
complete -c checkinstall -x -l rpmflags -d "Pass these flags to the rpm installer"
complete -c checkinstall -f -l rpmi -d "Use the -i flag for rpm when installing a .rpm"
complete -c checkinstall -f -l rpmu -d "Use the -U flag for rpm when installing a .rpm"
complete -c checkinstall -x -l dpkgflags -d "Pass these flags to the dpkg installer"
complete -c checkinstall -x -l spec -a "(__fish_complete_path)" -d ".spec file location"
complete -c checkinstall -f -l nodoc -d "Do not include documentation files"

# Info display options
complete -c checkinstall -x -o d -a "0 1 2" -d "Set debug level"
complete -c checkinstall -f -o si -d "Run an interactive install command"
complete -c checkinstall -x -l showinstall -a "$yn" -d "Toggle interactive install command"
complete -c checkinstall -f -o ss -d "Run an interactive Slackware installation script"
complete -c checkinstall -x -l showslack -a "$yn" -d "Toggle interactive Slackware installation script"

# Package tuning options
complete -c checkinstall -x -l autodoinst -a "$yn" -d "Toggle the creation of a doinst.sh script"
complete -c checkinstall -x -l strip -a "$yn" -d "Strip any ELF binaries found inside the package"
complete -c checkinstall -x -l stripso -a "$yn" -d "Strop any ELF binary libraries (.so files)"
complete -c checkinstall -x -l addso -a "$yn" -d "Search for ant shared libs and add them to /etc/ld.so.conf"
complete -c checkinstall -x -l reset-uids -a "$yn" -d "Reset perms for all files"
complete -c checkinstall -x -l gzman -a "$yn" -d "Compress any man pages found inside the package"
complete -c checkinstall -x -l docdir -a "(__fish_complete_directories)" -d "Where to put documentation files"
complete -c checkinstall -x -l umask -d "Set the umask value"
complete -c checkinstall -x -l exclude -a "(__fish_complete_path)" -d "Excluse these files/directories from the package"
complete -c checkinstall -F -r -l include -d "Include file/directories in this file in the package"
complete -c checkinstall -f -l inspect -d "Inspect the package's file list"
complete -c checkinstall -f -l review-spec -d "Review the dpec file before creating a .rpm"
complete -c checkinstall -f -l review-control -d "Review the control file before creating a .deb"
complete -c checkinstall -f -l newslack -d "Use the new (8.1+) Slackware description format"
complete -c checkinstall -F -l with-tar -d "Manually set the path to the tar binary"

# Cleanup options
complete -c checkinstall -x -l deldoc -a "$yn" -d "Delete doc-pak upon termination"
complete -c checkinstall -x -l deldesc -a "$yn" -d "Delete description-pak upon termination"
complete -c checkinstall -x -l delspec -a "$yn" -d "Delete spec file upon termination"
complete -c checkinstall -f -l bk -d "Backup any overwritten files"
complete -c checkinstall -x -l backup -a "$yn" -d "Toggle backup"

# About checkinstall
complete -c checkinstall -f -o h -l help -d "Show help"
complete -c checkinstall -f -l copyright -d "Show Copyright information"
complete -c checkinstall -f -l version -d "Show version information"
