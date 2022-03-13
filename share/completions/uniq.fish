if uniq --version >/dev/null 2>/dev/null
    complete -c uniq -s c -l count -d "Print number of occurrences"
    complete -c uniq -s d -l repeated -d "Only print duplicates"
    complete -c uniq -s D -l all-repeated -d "Remove non-duplicate lines" -f -x -a "
		none\t'Remove none-duplicate lines'
		prepend\t'Remove non-duplicate lines and print an empty line before each non-duplicate'
		separate\t'Remove non-duplicate lines and print an empty line between each non-duplicate'
	"
    complete -c uniq -s f -l skip-fields -d "Avoid comparing first N fields" -r
    complete -c uniq -s i -l ignore-case -d "Case insensitive"
    complete -c uniq -s s -l skip-chars -d "Avoid comparing first N characters" -r
    complete -c uniq -s u -l unique -d "Only print unique lines"
    complete -c uniq -s w -l check-chars -d "Compare only specified number of characters" -r
    complete -c uniq -l help -d "Display help and exit"
    complete -c uniq -l version -d "Display version and exit"
else # BSD
    complete -c uniq -s c -d 'Precede each output line with count of its occurrence'
    complete -c uniq -s d -d 'Only print duplicates'
    complete -c uniq -s f -d 'Avoid comparing first N fields' -x
    complete -c uniq -s s -d 'Avoid comparing first N characters' -x
    complete -c uniq -s u -d 'Only print unique lines'
    complete -c uniq -s i -d 'Case insensitive comparision'
end
