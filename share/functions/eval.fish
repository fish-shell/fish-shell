
function eval -S -d "Evaluate parameters as a command"
	echo begin\; $argv \;end eval2_inner \<\&3 3\<\&- | . 3<&0
end
