# Returns whether we *should* complete a -s or --long argument.
# The preference is NOT to do so, i.e. prefer subcommands over switches.
function __fish_should_complete_switches
	if not __fish_can_complete_switches
		return 1
	end
	if string match -qr -- "^-" (commandline -ct)[-1]
		return 0
	end

	return 1
end
