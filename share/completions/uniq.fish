complete -c uniq -s c -l count -d (N_ "Print number of occurences")
complete -c uniq -s d -l repeated -d (N_ "Only print duplicates")
complete -c uniq -s D -l all-repeated -d (N_ "Remove non-duplicate lines") -f -x -a "
	none\t'Remove none-duplicate lines'
	prepend\t'Remove non-duplicate lines and print an empty line before each non-duplicate'
	separate\t'Remove non-duplicate lines and print an empty line between each non-duplicate'
"
complete -c uniq -s f -l skip-fields -d (N_ "Avoid comparing first N fields") -r
complete -c uniq -s i -l ignore-case -d (N_ "Case insensitive")
complete -c uniq -s s -l skip-chars -d (N_ "Avoid comparing first N characters") -r
complete -c uniq -s u -l unique -d (N_ "Only print unique lines")
complete -c uniq -s w -l check-chars -d (N_ "Compare only specified number of characters") -r
complete -c uniq -l help -d (N_ "Display help and exit")
complete -c uniq -l version -d (N_ "Display version and exit")

