# Atom Editor v1.0.2

# Usage: atom [options] [path ...]

# One or more paths to files or folders may be specified. If there is an
# existing Atom window that contains all of the given folders, the paths
# will be opened in that window. Otherwise, they will be opened in a new
# window.

# Options:
#   -1, --one                  This option is no longer supported.                           [boolean]
#   --include-deprecated-apis  This option is not currently supported.                       [boolean]
#   -d, --dev                  Run in development mode.                                      [boolean]
complete -c atom -s d -l dev -d "Run in development mode."
#   -f, --foreground           Keep the browser process in the foreground.                   [boolean]
complete -c atom -s f -l foreground -d "Keep the browser process in the foreground."
#   -h, --help                 Print this usage message.                                     [boolean]
complete -c atom -s h -l help -d "Print usage message."
#   -l, --log-file             Log all output to file.                                        [string]
complete -c atom -s l -l log-file -r -d "Log all output to file."
#   -n, --new-window           Open a new window.                                            [boolean]
complete -c atom -s n -l new-window -d "Open a new window."
#   --profile-startup          Create a profile of the startup execution time.               [boolean]
complete -c atom -l profile-startup -d "Create a profile of the startup execution time."
#   -r, --resource-path        Set the path to the Atom source directory and enable dev-mode. [string]
complete -c atom -s r -l resource-path -d "Set the path to the Atom source directory and enable dev-mode."
#   -s, --spec-directory       Set the directory from which to run package specs (default: Atom's spec
#                              directory).                                                    [string]
complete -c atom -s s -l spec-directory -d "Set the directory from which to run package specs (default: Atom's spec directory)."
#   --safe                     Do not load packages from ~/.atom/packages or ~/.atom/dev/packages. [boolean]
complete -c atom -l safe -d "Do not load packages from ~/.atom/packages or ~/.atom/dev/packages."
#   -t, --test                 Run the specified specs and exit with error code on failures. [boolean]
complete -c atom -s t -l test -d "Run the specified specs and exit with error code on failures."
#   -v, --version              Print the version.                                            [boolean]
complete -c atom -s v -l version -f -d "Print the version."
#   -w, --wait                 Wait for window to be closed before returning.                [boolean]
complete -c atom -s w -l wait -d "Wait for window to be closed before returning."
