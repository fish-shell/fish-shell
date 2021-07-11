complete black -x -s c -l code -d 'Format code passed in as a string'
complete black -x -s l -l line-length -d 'Characters per line'
complete black -x -s t -l target-version -d 'Python versions supported by output'
complete black -l pyi -d 'Format all input files like typing stubs regardless of file extension'
complete black -s S -l skip-string-normalization -d "Don't normalize string quotes or prefixes"
complete black -s C -l skip-magic-trailing-comma -d "Don't use trailing commas as a reason to split lines"
complete black -l check -d "Don't write the files back, just return the status"
complete black -l diff -d "Don't write the files back, just output a diff for each file"
complete black -l color -d 'Show colored diff'
complete black -l no-color -d 'Do not color diff output'
complete black -l fast -d 'Skip temporary sanity checks'
complete black -l safe -d 'Do not skip temporary sanity checks'
complete black -x -l required-version -d 'Require a specific version of Black to be running'
complete black -r -l include -d 'Regular expression of items to include'
complete black -r -l exclude -d 'Regular expression of items to exclude'
complete black -r -l extend-exclude -d 'Regular expression of additional items to exclude'
complete black -r -l force-exclude -d 'Regular expression of Items to always exclude'
complete black -r -l stdin-filename -d 'The name of the file when passing through stdin'
complete black -s q -l quiet -d 'Only print error messages to stderr'
complete black -s v -l verbose -d 'Report files that were unchanged or ignored'
complete black -l version -d 'Show version'
complete black -r -l config -d 'Configuration file'
complete black -s h -l help -d 'Show help'
