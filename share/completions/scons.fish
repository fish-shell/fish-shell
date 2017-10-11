#
# Command specific completions for the scons command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

complete -c scons -s c -l clean -l remove -d 'Clean up by removing all target files for which a construction command is specified'
complete -c scons -l cache-debug -d 'Print debug information about the CacheDir() derived-file caching to the specified file'
complete -c scons -l cache-disable -l no-cache -d 'Disable the derived-file caching specified by CacheDir()'
complete -c scons -l cache-force -l cache-populate -d 'When using CacheDir(), populate a cache by copying any already- existing, up-to-date derived files to the cache, in addition to files built by this invocation'
complete -c scons -l cache-show -d 'When using CacheDir() and retrieving a derived file from the cache, show the command that would have been executed to build the file, instead of the usual report, "Retrieved file from cache'

complete -c scons -l config -d 'This specifies how the Configure call should use or generate the results of configuration tests' -a '
	auto\t"Use normal dependency mechanism"
	force\t"Rerun all tests"
	cache\t"Take all results from cache"' -x

complete -c scons -s C -d 'Directory, --directory=directory Change to the specified directory before searching for the SCon struct, Sconstruct, or sconstruct file, or doing anything else'
complete -c scons -s D -d 'Works exactly the same way as the -u option except for the way default targets are handled'

complete -c scons -l debug -d 'Debug the build process' -a "count dtree explain findlibs includes memoizer memory nomemoizer objects pdb presub stacktrace stree time tree" -x

complete -c scons -l diskcheck -d 'Enable specific checks for whether or not there is a file on disk where the SCons configuration expects a directory (or vice versa), and whether or not RCS or SCCS sources exist when searching for source and include files' -a "all none match rcs " -x

complete -c scons -s f -l file -l makefile -l sconstruct -d 'Use file as the initial SConscript file'
complete -c scons -s h -l help -d 'Print a local help message for this build, if one is defined in the SConscript file(s), plus a line that describes the -H option for command-line option help'
complete -c scons -s H -l help-options -d 'Print the standard help message about command-line options and exit'
complete -c scons -s i -l ignore-errors -d 'Ignore all errors from commands executed to rebuild files'
complete -c scons -s I -l include-dir -d 'Specifies a directory to search for imported Python modules'
complete -c scons -l implicit-cache -d 'Cache implicit dependencies'
complete -c scons -l implicit-deps-changed -d 'Force SCons to ignore the cached implicit dependencies'
complete -c scons -l implicit-deps-unchanged -d 'Force SCons to ignore changes in the implicit dependencies'
complete -c scons -s j -l jobs -d 'Specifies the number of jobs (commands) to run simultaneously'
complete -c scons -s k -l keep-going -d 'Continue as much as possible after an error'
complete -c scons -l duplicate -d 'There are three ways to duplicate files in a build tree: hard links, soft (symbolic) links and copies'
complete -c scons -l max-drift -d 'Set the maximum expected drift in the modification time of files to SECONDS'
complete -c scons -s n -l just-print -l dry-run -l recon -d 'No execute'
complete -c scons -l profile -d 'Run SCons under the Python profiler and save the results in the specified file'
complete -c scons -s q -l question -d 'Do not run any commands, or print anything'
complete -c scons -s Q -d 'Quiets SCons status messages about reading SConscript files, building targets and entering directories'
complete -c scons -l random -d 'Build dependencies in a random order'
complete -c scons -s s -l silent -l quiet -d 'Silent'
complete -c scons -l taskmastertrace -d 'Prints trace information to the specified file about how the internal Taskmaster object evaluates and controls the order in which Nodes are built'
complete -c scons -s u -l up -l search-up -d 'Walks up the directory structure until an SConstruct , Scon struct or sconstruct file is found, and uses that as the top of the directory tree'
complete -c scons -s U -d 'Works exactly the same way as the -u option except for the way default targets are handled'
complete -c scons -s v -l version -d 'Print the scons version, copyright information, list of authors, and any other relevant information'
complete -c scons -s w -l print-directory -d 'Print a message containing the working directory before and after other processing'

complete -c scons -l warn -d 'Enable or disable warnings' -a 'all no-all dependency no-dependency deprecated no-deprecated missing-sconscript no-missing-sconscript' -x

complete -c scons -l no-print-directory -d 'Turn off -w, even if it was turned on implicitly'
complete -c scons -s Y -l repository -d 'Search the specified repository for any input and target files not found in the local directory hierarchy'
