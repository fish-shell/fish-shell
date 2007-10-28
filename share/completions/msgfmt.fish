
complete -c msgfmt -s D -l directory --description "Add specified directory to list for input files search" -x -a "(__fish_complete_directories (commandline -ct))"

set -l cond "not __fish_contains_opt -s j java java2 csharp csharp-resource tcl qt"
complete -c msgfmt -n $cond -s j -l java --description "Generate a Java ResourceBundle class"
complete -c msgfmt -n $cond -l java2 --description "Like --java, and assume Java2 (JDK 1.2 or higher)"
complete -c msgfmt -n $cond -l csharp --description "Generate a .NET .dll file"
complete -c msgfmt -n $cond -l csharp-resources --description "Generate a .NET .resources file"
complete -c msgfmt -n $cond -l tcl --description "Generate a tcl/msgcat .msg file"
complete -c msgfmt -n $cond -l qt --description "Generate a Qt .qm file"

complete -c msgfmt -s o -l output-file --description "Write output to specified file" -r
complete -c msgfmt -l strict --description "Enable strict Uniforum mode"


set -l cond "__fish_contains_opt -s j java java2 csharp csharp-resource tcl"
complete -c msgfmt -n $cond -s r -l resource --description "Resource name"
complete -c msgfmt -n $cond -s l -l locale --description "Locale name, either language or language_COUNTRY"
complete -c msgfmt -n $cond -s d --description "Base directory for output" -x -a "(__fish_complete_directories (commandline -ct))"

complete -c msgfmt -s P -l properties-input --description "Input files are in Java .properties syntax"
complete -c msgfmt -l stringtable-input --description "Input files are in NeXTstep/GNUstep .strings syntax"
complete -c msgfmt -s c -l check --description "Perform all the checks implied by --check-format, --check-header, --check-domain"
complete -c msgfmt -l check-format --description "Check language dependent format strings"
complete -c msgfmt -l chack-header --description "Verify presence and contents of the header entry"
complete -c msgfmt -l check-domain --description "Check for conflicts between domain directives and the --output-file option"
complete -c msgfmt -s C -l check-compatibility --description "Check that GNU msgfmt behaves like X/Open msgfmt"
complete -c msgfmt -l check-accelerators --description "Check presence of keyboard accelerators for menu items"
complete -c msgfmt -s f -l use-fuzzy --description "Use fuzzy entries in output"
complete -c msgfmt -s a -l alignment --description "Alignment" -r
complete -c msgfmt -l no-hash --description "Binary file will not include the hash table"
complete -c msgfmt -s h -l help --description "Display help and exit"
complete -c msgfmt -s V -l version --description "Display version and exit"
complete -c msgfmt -l statistics --description "Print statistics about translations"
complete -c msgfmt -l verbose --description "Increase verbosity level"

