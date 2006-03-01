
function dirh -d (N_ "Print the current directory history (the back- and fwd- lists)") 
	# Avoid set comment
	set -l current (command pwd)
	set -l separator "  "
	set -l line_len (echo (count $dirprev) + (echo $dirprev $current $dirnext | wc -m) | bc)
	if test $line_len -gt $COLUMNS
		# Print one entry per line if history is long
		set separator "\n"
	end

	for i in $dirprev
		echo -n -e $i$separator
	end

	set_color $fish_color_history_current
	echo -n -e $current$separator
	set_color normal
	
	for i in (seq (echo (count $dirnext)) -1 1)
		echo -n -e $dirnext[$i]$separator
	end

	echo
end

