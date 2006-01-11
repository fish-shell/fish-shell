
function __fish_complete_man
	if test (commandline -ct)

		# Try to guess what section to search in. If we don't know, we
		# use [^)]*, which should match any section

		set section ""
		set prev (commandline -poc)
		set -e prev[1]
		while count $prev
			switch $prev[1]
			case '-**'
				
			case '*'
				set section $prev[1]
			end
			set -e prev[1]
		end
		
		set section $section"[^)]*"

		# Do the actual search
		apropos (commandline -ct) | grep \^(commandline -ct) | sed -n -e 's/\([^ ]*\).*(\('$section'\)) *- */\1\t\2: /p'
	end
end



complete -xc man -a "(__fish_complete_man)"

complete -xc man -a 1 -d (_ "Program section")
complete -xc man -a 2 -d (_ "Syscall section")
complete -xc man -a 3 -d (_ "Library section")
complete -xc man -a 4 -d (_ "Device section")
complete -xc man -a 5 -d (_ "File format section")
complete -xc man -a 6 -d (_ "Games section")
complete -xc man -a 7 -d (_ "Misc section")
complete -xc man -a 8 -d (_ "Admin section")
complete -xc man -a 9 -d (_ "Kernel section")
complete -xc man -a tcl -d (_ "Tcl section")
complete -xc man -a n -d (_ "New section")
complete -xc man -a l -d (_ "Local section")
complete -xc man -a p
complete -xc man -a o -d (_ "Old section")
complete -rc man -s C -d (_ "Configuration file")
complete -xc man -s M -a "(__fish_complete_directory (commandline -ct))" -d (_ "Manpath")
complete -rc man -s P -d (_ "Pager")
complete -xc man -s S -d (_ "Manual sections")
complete -c man -s a -d (_ "Display all matches")
complete -c man -s c -d (_ "Always reformat")
complete -c man -s d -d (_ "Debug")
complete -c man -s D -d (_ "Debug and run")
complete -c man -s f -d (_ "Show whatis information")
complete -c man -s F -l preformat -d (_ "Format only")
complete -c man -s h -d (_ "Display help and exit")
complete -c man -s k -d (_ "Show apropos information")
complete -c man -s K -d (_ "Search in all man pages")
complete -xc man -s m -d (_ "Set system")
complete -xc man -s p -d (_ "Preprocessors")
complete -c man -s t -d (_ "Format for printing")
complete -c man -s w -l path -d (_ "Only print locations")
complete -c man -s W -d (_ "Only print locations")

