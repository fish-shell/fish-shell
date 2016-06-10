if uniq --version > /dev/null ^ /dev/null
	complete -c uniq -s c -l count --description "Print number of occurences"
	complete -c uniq -s d -l repeated --description "Only print duplicates"
	complete -c uniq -s D -l all-repeated --description "Remove non-duplicate lines" -f -x -a "
		none\t'Remove none-duplicate lines'
		prepend\t'Remove non-duplicate lines and print an empty line before each non-duplicate'
		separate\t'Remove non-duplicate lines and print an empty line between each non-duplicate'
	"
	complete -c uniq -s f -l skip-fields --description "Avoid comparing first N fields" -r
	complete -c uniq -s i -l ignore-case --description "Case insensitive"
	complete -c uniq -s s -l skip-chars --description "Avoid comparing first N characters" -r
	complete -c uniq -s u -l unique --description "Only print unique lines"
	complete -c uniq -s w -l check-chars --description "Compare only specified number of characters" -r
	complete -c uniq -l help --description "Display help and exit"
	complete -c uniq -l version --description "Display version and exit"
else # OS X
	complete -c uniq -s c -d 'Precede each output line with count of it\'s occurrence'
	complete -c uniq -s d -d 'Only print duplicates'
	complete -c uniq -s f -d 'Avoid comparing first N fields' -x
	complete -c uniq -s s -d 'Avoid comparing fist N characters' -x
	complete -c uniq -s u -d 'Only print unique lines'
	complete -c uniq -s i -d 'Case insensitive comparision'
end
