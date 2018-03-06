
# magic completion safety check (do not remove this comment)
if not type -q cat
    exit
end
if cat --version ^ /dev/null > /dev/null # GNU
	complete -c cat -s A -l show-all -d "Escape all non-printing characters"
	complete -c cat -s b -l number-nonblank -d "Number nonblank lines"
	complete -c cat -s e -d "Escape non-printing characters except tab"
	complete -c cat -s E -l show-ends -d "Display \$ at end of line"
	complete -c cat -s n -l number -d "Number all lines"
	complete -c cat -s s -l squeeze-blank -d "Never more than single blank line"
	complete -c cat -s t -d "Escape non-printing characters except newline"
	complete -c cat -s T -l show-tabs -d "Escape tab"
	complete -c cat -s v -d "Escape non-printing except newline and tab"
	complete -c cat -l help -d "Display help and exit"
	complete -c cat -l version -d "Display version and exit"
else # OS X
	complete -c cat -s b -d "Number non-blank lines"
	complete -c cat -s e -d "Display non-printing characters, and `\$' at the end of each line"
	complete -c cat -s n -d "Number all lines"
	complete -c cat -s s -d "Single spaced output by squeezing adjacent empty lines"
	complete -c cat -s t -d "Display non-printing characters, and tab characters as `^I'"
	complete -c cat -s u -d "Disable output buffering"
	complete -c cat -s v -d "Display non-printing characters so they're visible."
end
