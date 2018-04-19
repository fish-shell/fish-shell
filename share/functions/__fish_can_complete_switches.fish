# Returns whether it is at all possible (even if not recommended)
# to complete a -s or --long argument.
function __fish_can_complete_switches
	# Search backwards
	for arg in (commandline -ct)[-1..1]
		if test "$arg" = ""
			continue
		else if not string match -qr -- "^-\S*\$" "$arg"
			return 1
		end
	end

	return 0
end
