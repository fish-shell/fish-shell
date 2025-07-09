# fish completion for Perl's cpan

function __fish_cpan_list_installed_modules
    # Following IRC's #fish suggestion to use </dev/null as cpan might go interactive
    cpan -l </dev/null | while read -l line
        # Filter out unrelated messages or notifications
        if string match -qr -- '^\w.*\t\w.*$' $line
            string replace -r -- '\t.*' '' $line |
                string escape --style=script
        end
    end
end

complete -c cpan -s a -d "Creates a CPAN.pm autobundle with CPAN::Shell->autobundle"
complete -c cpan -s A -d "Show primary maintainer for specified module" -xa "(__fish_cpan_list_installed_modules)"
complete -c cpan -s c -d "Runs a `make clean` in the specified modules directories"
complete -c cpan -s C -d "Show the Changes files for specified module" -a "(__fish_cpan_list_installed_modules)"
complete -c cpan -s D -d "Show module details" -a "(__fish_cpan_list_installed_modules)"
complete -c cpan -s f -d "Force the specified action"
complete -c cpan -s F -d "Turn off CPAN.pm's attempts to lock anything"
complete -c cpan -s g -d "Download latest distribution of module to current directory" -a "(__fish_cpan_list_installed_modules)"
# complete -c cpan -s G -d "UNIMPLEMENTED"
complete -c cpan -s h -d "Print a help message and exit"
complete -c cpan -s i -d "Install specified module"
complete -c cpan -s I -d "Load 'local::lib' (think like '-I' for loading lib paths)"
complete -c cpan -s j -d "Load file with CPAN configuration data"
complete -c cpan -s J -d "Dump the configuration in the same format that CPAN.pm uses"
complete -c cpan -s l -d "list all installed modules with their versions"
complete -c cpan -s L -d "List the modules by the specified authors"
complete -c cpan -s m -d "Make the specified modules"
complete -c cpan -s M -d "Comma-separated list of mirrors to use for this run" -x
#complete -c cpan -s n -d "Do a dry run, but dont actually install anything. (unimplemented)"
complete -c cpan -s O -d "Show the out-of-date modules"
complete -c cpan -s p -d "Ping the configured mirrors and print a report"
complete -c cpan -s P -d "Find and the best mirrors available"
complete -c cpan -s r -d "Recompiles dynamically loaded modules with CPAN::Shell->recompile"
complete -c cpan -s s -d "Drop in the CPAN.pm shell"
complete -c cpan -s t -d "Run a `make test` on the specified modules"
complete -c cpan -s T -d "Do not test modules. Simply install them"
complete -c cpan -s u -d "Upgrade all installed modules"
complete -c cpan -s v -d "Print the script version and CPAN.pm version then exit"
complete -c cpan -s V -d "Print detailed information about the cpan client"
# complete -c cpan -s w -d "UNIMPLEMENTED"
complete -c cpan -s x -d "Find close matches to named module. Requires Text::Levenshtein or others"
complete -c cpan -s X -d "Dump all the namespaces to standard output"
