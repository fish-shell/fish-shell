complete -c msgfmt -s D -l directory -d "Add specified directory to list for input files search" -x -a "(__fish_complete_directories (commandline -ct))"

set -l cond "not __fish_contains_opt -s j java java2 csharp csharp-resource tcl qt"
complete -c msgfmt -n $cond -s j -l java -d "Generate a Java ResourceBundle class"
complete -c msgfmt -n $cond -l java2 -d "Like --java, and assume Java2 (JDK 1.2 or higher)"
complete -c msgfmt -n $cond -l csharp -d "Generate a .NET .dll file"
complete -c msgfmt -n $cond -l csharp-resources -d "Generate a .NET .resources file"
complete -c msgfmt -n $cond -l tcl -d "Generate a tcl/msgcat .msg file"
complete -c msgfmt -n $cond -l qt -d "Generate a Qt .qm file"

complete -c msgfmt -s o -l output-file -d "Write output to specified file" -r
complete -c msgfmt -l strict -d "Enable strict Uniforum mode"

set -l cond "__fish_contains_opt -s j java java2 csharp csharp-resource tcl"
complete -c msgfmt -n $cond -s r -l resource -d "Resource name"
complete -c msgfmt -n $cond -s l -l locale -d "Locale name, either language or language_COUNTRY"
complete -c msgfmt -n $cond -s d -d "Base directory for output" -x -a "(__fish_complete_directories (commandline -ct))"

complete -c msgfmt -s P -l properties-input -d "Input files are in Java .properties syntax"
complete -c msgfmt -l stringtable-input -d "Input files are in NeXTstep/GNUstep .strings syntax"
complete -c msgfmt -s c -l check -d "Perform all the checks implied by --check-format, --check-header, --check-domain"
complete -c msgfmt -l check-format -d "Check language dependent format strings"
complete -c msgfmt -l chack-header -d "Verify presence and contents of the header entry"
complete -c msgfmt -l check-domain -d "Check for conflicts between domain directives and the --output-file option"
complete -c msgfmt -s C -l check-compatibility -d "Check that GNU msgfmt behaves like X/Open msgfmt"
complete -c msgfmt -l check-accelerators -d "Check presence of keyboard accelerators for menu items"
complete -c msgfmt -s f -l use-fuzzy -d "Use fuzzy entries in output"
complete -c msgfmt -s a -l alignment -d Alignment -r
complete -c msgfmt -l no-hash -d "Binary file will not include the hash table"
complete -c msgfmt -s h -l help -d "Display help and exit"
complete -c msgfmt -s V -l version -d "Display version and exit"
complete -c msgfmt -l statistics -d "Print statistics about translations"
complete -c msgfmt -l verbose -d "Increase verbosity level"
