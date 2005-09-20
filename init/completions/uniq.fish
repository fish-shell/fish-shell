complete -c uniq -s c -l count -d "Print number of occurences"
complete -c uniq -s d -l repeated -d "Only print duplicates"
complete -c uniq -s D -l all-repeated -d "Remove non-suplicate lines" -f -x -a "
	none\t'Remove none-duplicate lines'
	prepend\t'Remove non-duplicate lines and print an empty line before each non-duplicate'
	separate\t'Remove non-duplicate lines and print an empty line between each non-duplicate'
"
complete -c uniq -s f -l skip-fields -d "Avoid comparing first N fields" -r
complete -c uniq -s i -l ignore-case -d "Case insensitive"
complete -c uniq -s s -l skip-chars -d "Avoid comparing first N characters" -r
complete -c uniq -s u -l unique -d "Only print unique lines"
complete -c uniq -s w -l check-chars -d "Compare only N characters" -r
complete -c uniq -l help -d "Display help and exit"
complete -c uniq -l version -d "Display version and exit"

