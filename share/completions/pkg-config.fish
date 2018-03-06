# pkg-config

# magic completion safety check (do not remove this comment)
if not type -q pkg-config
    exit
end
complete -f -c pkg-config -l modversion -d 'Print versions of the specified libraries'
complete -f -c pkg-config -l version -d 'Display the version of pkg-config and quit'
complete -f -c pkg-config -l help -d 'Displays a help message and quit'
complete -f -c pkg-config -l print-errors -d 'Print message when errors occur'
complete -f -c pkg-config -l silence-errors -d 'Stay quiet when errors occur'
complete -f -c pkg-config -l errors-to-stdout -d 'Print errors to stdout instead of stderr'
complete -f -c pkg-config -l debug -d 'Print debugging information'
complete -f -c pkg-config -l cflags -d 'Print pre-processor and compile flags for the specified libraries'
complete -f -c pkg-config -l cflags-only-I -d 'This prints the -I part of "--cflags".'
complete -f -c pkg-config -l libs -d 'Print link flags'
complete -f -c pkg-config -l libs-only-L -d 'This prints the -L/-R part of "--libs".'
complete -f -c pkg-config -l libs-only-l -d 'This prints the -l part of "--libs"'
# TODO: Something like 'pkg-config --print-variables $lib' should offer the options here - the difficulty is determining $lib
complete -f -c pkg-config -l variable -d 'This returns the value of a variable defined in a package\'s .pc file'
complete -f -c pkg-config -l define-variable -d 'This sets a global value for a variable'
complete -f -c pkg-config -l print-variables -d 'Returns a list of all variables defined in the package'
complete -f -c pkg-config -l uninstalled -d 'Return success if any -uninstalled packages are used'
complete -f -c pkg-config -l max-version -d 'Test if a package has at most the specified version'
complete -f -c pkg-config -l atleast-version -d 'Test if a package has at least this version'
complete -f -c pkg-config -l exact-version -d 'Test if a package has exactly this version'
complete -f -c pkg-config -l exists -d 'Test if a package exists'
complete -f -c pkg-config -l static -d 'Output libraries suitable for static linking'
complete -f -c pkg-config -l list-all -d 'List all modules found in the pkg-config path'
complete -f -c pkg-config -l print-provides -d 'List all modules the given packages provides'
complete -f -c pkg-config -l print-requires -d 'List all modules the given packages requires'
complete -f -c pkg-config -l print-requires-private -d 'List all modules the given packages requires for static linking'
complete -f -c pkg-config -a '(command pkg-config --list-all | string replace -r " (.*)" "\t\$1")'
