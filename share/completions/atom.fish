# Atom Editor v1.24.1

# Usage: atom [options] [path ...]

# One or more paths to files or folders may be specified. If there is an
# existing Atom window that contains all of the given folders, the paths
# will be opened in that window. Otherwise, they will be opened in a new
# window.

# Options:
#   -d, --dev                  Run in development mode.  [boolean]
complete -c atom -s d -l dev -d 'Run in development mode'
#   -f, --foreground           Keep the main process in the foreground.  [boolean]
complete -c atom -s f -l foreground -d 'Keep the main process in the foreground'
#   -h, --help                 Print this usage message.  [boolean]
complete -c atom -s h -l help -d 'Print usage message'
#   -l, --log-file             Log all output to file.  [string]
complete -c atom -s l -l log-file -r -d 'Log all output to file'
#   -n, --new-window           Open a new window.  [boolean]
complete -c atom -s n -l new-window -d 'Open a new window'
#   --profile-startup          Create a profile of the startup execution time.  [boolean]
complete -c atom -l profile-startup -d 'Create a profile of the startup execution time'
#   -r, --resource-path        Set the path to the Atom source directory and enable dev-mode.  [string]
complete -c atom -s r -l resource-path -r -d 'Set the path to the Atom source directory and enable dev mode'
#   --safe                     Do not load packages from ~/.atom/packages or ~/.atom/dev/packages.  [boolean]
complete -c atom -l safe -d 'Run in safe mode'
#   --benchmark                Open a new window that runs the specified benchmarks.  [boolean]
complete -c atom -l benchmark -d 'Run the specified benchmarks in a new window'
#   --benchmark--test          Run a faster version of the benchmarks in headless mode.
complete -c atom -l benchmark-test -d 'Run a faster version of the benchmarks in headless mode'
#   -t, --test                 Run the specified specs and exit with error code on failures.  [boolean]
complete -c atom -s t -l test -d 'Run the specified specs and exit with error code on failures'
#   -m, --main-process         Run the specified specs in the main process.  [boolean]
complete -c atom -s m -l main-process -d 'Run the specified specs in the main process'
#   --timeout                  When in test mode, waits until the specified time (in minutes) and kills the process (exit code: 130).  [string]
complete -c atom -l timeout -r -d 'When in test mode, waits until the specified time (in minutes) and kills the process'
#   -v, --version              Print the version information.  [boolean]
complete -c atom -s v -l version -d 'Print the version information'
#   -w, --wait                 Wait for window to be closed before returning.  [boolean]
complete -c atom -s w -l wait -d 'Wait for editor to be closed before returning'
#   --clear-window-state       Delete all Atom environment state.  [boolean]
complete -c atom -l clear-window-state -d 'Delete all Atom environment state'
#   --enable-electron-logging  Enable low-level logging messages from Electron.  [boolean]
complete -c atom -l enable-electron-logging -d 'Enable low-level logging messages from Electron'
#   -a, --add                  Open path as a new project in last used window.  [boolean]
complete -c atom -s a -l add -d 'Open path as a new project in last used window'
