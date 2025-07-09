# fish completion for Perl's cpan

function list_installed_modules
    # Following IRC's #fish suggestion to use </dev/null as cpan might go interactive
    cpan -l </dev/null | while read -l line
		# Filter out unrelated messages or notifications
		if string match -qr -- '^\w.*\t\w.*$' $line
			string replace -r -- '\t(.*)' '' $line | \
			string escape --style=script
		end
	end
end

set -lx installed_modules (list_installed_modules | string collect)

complete -c cpan -o a -d "Creates a CPAN.pm autobundle with CPAN::Shell->autobundle"
complete -c cpan -o A -d "Show primary maintainer for specified module" -a $installed_modules
complete -c cpan -o c -d "Runs a `make clean` in the specified modules directories"
complete -c cpan -o C -d "Show the Changes files for specified module" -a "$installed_modules"
complete -c cpan -o D -d "Show module details" -a "$installed_modules"
complete -c cpan -o f -d "Force the specified action"
complete -c cpan -o F -d "Turn off CPAN.pm's attempts to lock anything"
complete -c cpan -o g -d "Download latest distribution of module to current directory" -a $installed_modules
# complete -c cpan -o G -d "UNIMPLEMENTED"
complete -c cpan -o h -d "Print a help message and exit"
complete -c cpan -o i -d "Install specified module"
complete -c cpan -o I -d "Load 'local::lib' (think like '-I' for loading lib paths)"
complete -c cpan -o j -d "Load file with CPAN configuration data"
complete -c cpan -o J -d "Dump the configuration in the same format that CPAN.pm uses"
complete -c cpan -o l -d "list all installed modules with their versions"
complete -c cpan -o L -d "List the modules by the specified authors"
complete -c cpan -o m -d "Make the specified modules"
complete -c cpan -o M -d "Comma-separated list of mirrors to use for this run"
#complete -c cpan -o n -d "Do a dry run, but dont actually install anything. (unimplemented)"
complete -c cpan -o O -d "Show the out-of-date modules"
complete -c cpan -o p -d "Ping the configured mirrors and print a report"
complete -c cpan -o P -d "Find and the best mirrors available"
complete -c cpan -o r -d "Recompiles dynamically loaded modules with CPAN::Shell->recompile"
complete -c cpan -o s -d "Drop in the CPAN.pm shell"
complete -c cpan -o t -d "Run a `make test` on the specified modules"
complete -c cpan -o T -d "Do not test modules. Simply install them"
complete -c cpan -o u -d "Upgrade all installed modules"
complete -c cpan -o v -d "Print the script version and CPAN.pm version then exit"
complete -c cpan -o V -d "Print detailed information about the cpan client"
# complete -c cpan -o w -d "UNIMPLEMENTED"
complete -c cpan -o x -d "Find close matches to named module. Requires Text::Levenshtein or others"
complete -c cpan -o X -d "Dump all the namespaces to standard output"
