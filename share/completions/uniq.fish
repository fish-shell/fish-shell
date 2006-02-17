complete -c uniq -s c -l count -d (_ "Print number of occurences")
complete -c uniq -s d -l repeated -d (_ "Only print duplicates")
complete -c uniq -s D -l all-repeated -d (_ "Remove non-duplicate lines") -f -x -a "
	none\t'Remove none-duplicate lines'
	prepend\t'Remove non-duplicate lines and print an empty line before each non-duplicate'
	separate\t'Remove non-duplicate lines and print an empty line between each non-duplicate'
"
complete -c uniq -s f -l skip-fields -d (_ "Avoid comparing first N fields") -r
complete -c uniq -s i -l ignore-case -d (_ "Case insensitive")
complete -c uniq -s s -l skip-chars -d (_ "Avoid comparing first N characters") -r
complete -c uniq -s u -l unique -d (_ "Only print unique lines")
complete -c uniq -s w -l check-chars -d (_ "Compare only specified number of characters") -r
complete -c uniq -l help -d (_ "Display help and exit")
complete -c uniq -l version -d (_ "Display version and exit")

