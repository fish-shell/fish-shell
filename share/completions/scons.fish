complete -c scons -s c -l clean -l remove -d 'Clean up all target files'
complete -c scons -l cache-debug -d 'Print debug information about the CacheDir() derived-file caching to the specified file'
complete -c scons -l cache-disable -l no-cache -d 'Disable the derived-file caching specified by CacheDir()'
complete -c scons -l cache-force -l cache-populate -d 'Populate cache with already existing files'
complete -c scons -l cache-show -d 'Show how a cached file would be built'

complete -c scons -l config -d 'How the Configure call should run the config tests' -a '
	auto\t"Use normal dependency mechanism"
	force\t"Rerun all tests"
	cache\t"Take all results from cache"' -x

complete -c scons -s C -l directory -d 'Change to this directory before searching for the sconstruct file'
complete -c scons -s D -d 'Like -u except for the way default targets are handled'

complete -c scons -l debug -d 'Debug the build process' -a "count dtree explain findlibs includes memoizer memory nomemoizer objects pdb presub stacktrace stree time tree" -x

complete -c scons -l diskcheck -d 'Check if files and directories are where they should be' -a "all none match rcs " -x

complete -c scons -s f -l file -l makefile -l sconstruct -d 'Use file as the initial SConscript file'
complete -c scons -s h -l help -d 'Print a help message for this build'
complete -c scons -s H -l help-options -d 'Print the standard help message about command-line options and exit'
complete -c scons -s i -l ignore-errors -d 'Ignore all errors from commands executed to rebuild files'
complete -c scons -s I -l include-dir -d 'Specifies a directory to search for imported Python modules'
complete -c scons -l implicit-cache -d 'Cache implicit dependencies'
complete -c scons -l implicit-deps-changed -d 'Force SCons to ignore the cached implicit dependencies'
complete -c scons -l implicit-deps-unchanged -d 'Force SCons to ignore changes in the implicit dependencies'
complete -c scons -s j -l jobs -d 'Specifies the number of jobs (commands) to run simultaneously'
complete -c scons -s k -l keep-going -d 'Continue as much as possible after an error'
complete -c scons -l duplicate -d 'How to duplicate files' -a 'hard-soft-copy soft-hard-copy hard-copy soft-copy copy'
complete -c scons -l max-drift -d 'Set the maximum expected drift in the modification time of files to SECONDS'
complete -c scons -s n -l just-print -l dry-run -l recon -d 'No execute'
complete -c scons -l profile -d 'Run SCons under the Python profiler and save the results in the specified file'
complete -c scons -s q -l question -d 'Do not run any commands, or print anything'
complete -c scons -s Q -d 'Quiet some status messages'
complete -c scons -l random -d 'Build dependencies in a random order'
complete -c scons -s s -l silent -l quiet -d Silent
complete -c scons -l taskmastertrace -d 'Prints Taskmaster trace information to the specified file'
complete -c scons -s u -l up -l search-up -d 'Walks up directories for an SConstruct file, and uses that as the top of the directory tree'
complete -c scons -s U -d 'Like -u option except for how default targets are handled'
complete -c scons -s v -l version -d 'Print the scons version information'
complete -c scons -s w -l print-directory -d 'Print the working directory'

complete -c scons -l warn -d 'Enable or disable warnings' -a 'all no-all dependency no-dependency deprecated no-deprecated missing-sconscript no-missing-sconscript' -x

complete -c scons -l no-print-directory -d 'Turn off -w, even if it was turned on implicitly'
complete -c scons -s Y -l repository -d 'Search this repo for input and target files not in the local directory tree'
