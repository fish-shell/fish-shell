complete -c black -x -s c -l code -d 'Format code passed in as a string'
complete -c black -x -s l -l line-length -d 'Characters per line'
complete -c black -x -s t -l target-version -d 'Python versions supported by output'
complete -c black -l pyi -d 'Format all input files like typing stubs regardless of file extension'
complete -c black -s S -l skip-string-normalization -d "Don't normalize string quotes or prefixes"
complete -c black -s C -l skip-magic-trailing-comma -d "Don't use trailing commas as a reason to split lines"
complete -c black -l check -d "Don't write the files back, just return the status"
complete -c black -l diff -d "Don't write the files back, just output a diff for each file"
complete -c black -l color -d 'Show colored diff'
complete -c black -l no-color -d 'Do not color diff output'
complete -c black -l fast -d 'Skip temporary sanity checks'
complete -c black -l safe -d 'Do not skip temporary sanity checks'
complete -c black -x -l required-version -d 'Require a specific version of Black to be running'
complete -c black -r -l include -d 'Regular expression of items to include'
complete -c black -r -l exclude -d 'Regular expression of items to exclude'
complete -c black -r -l extend-exclude -d 'Regular expression of additional items to exclude'
complete -c black -r -l force-exclude -d 'Regular expression of Items to always exclude'
complete -c black -r -l stdin-filename -d 'The name of the file when passing through stdin'
complete -c black -s q -l quiet -d 'Only print error messages to stderr'
complete -c black -s v -l verbose -d 'Report files that were unchanged or ignored'
complete -c black -l version -d 'Show version'
complete -c black -r -l config -d 'Configuration file'
complete -c black -s h -l help -d 'Show help'
