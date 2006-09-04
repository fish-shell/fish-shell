
function math -d (N_ "Perform math calculations in bc")
	if count $argv >/dev/null
		set -l out (echo $argv|bc)
		echo $out
		switch $out
			case 0
				return 1
		end
		return 0
	end
	return 2
	
end

