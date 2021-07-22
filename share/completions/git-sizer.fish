# Completions for git-sizer

complete -f -c git-sizer -l help -d "Display help for Git Sizer"
complete -f -c git-sizer -s v -l verbose -d "Report all statistics"
complete -x -c git-sizer -l threshold -d "Minimum level of concern"
complete -f -c git-sizer -l critical -d "Only report critical statistics"
complete -x -c git-sizer -l names -d "How to display names of large objects" -a "
	none\t'Omit footnotes entirely'
	hash\t'Only show SHA-1s'
	full\t'Show full names'
"
complete -f -c git-sizer -s j -l json -d "Output results in JSON format"
complete -x -c git-sizer -l json-version -d "JSON format version to use" -a "1 2"
complete -f -c git-sizer -l progress -d "Report progress to stderr"
complete -f -c git-sizer -l no-progress -d "Don't report progress to stderr"
complete -f -c git-sizer -l version -d "Output git-sizer version"
complete -x -c git-sizer -l branches -d "Process branches"
complete -x -c git-sizer -l tags -d "Process tags"
complete -x -c git-sizer -l remotes -d "Process remote refs"
complete -x -c git-sizer -l include -d "Process refs with specified PREFIX"
complete -x -c git-sizer -l exclude -d "Don't process refs with specified PREFIX"
complete -x -c git-sizer -l include-regexp -d "Process refs matching specified regex"
complete -x -c git-sizer -l exclude-regexp -d "Don't process refs matching specified regex"
complete -f -c git-sizer -l show-refs -d "Show refs which are being included/excluded"
