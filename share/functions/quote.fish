function quote -d "Echo a variable in a way that keeps it a list."
	for arg in $argv
		echo $arg
	end
end
