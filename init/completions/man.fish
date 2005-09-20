
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

complete -xc man -a 1 -d "Program section"
complete -xc man -a 2 -d "Syscall section"
complete -xc man -a 3 -d "Library section"
complete -xc man -a 4 -d "Device section"
complete -xc man -a 5 -d "File format section"
complete -xc man -a 6 -d "Games section"
complete -xc man -a 7 -d "Misc section"
complete -xc man -a 8 -d "Admin section"
complete -xc man -a 9 -d "Kernel section"
complete -xc man -a tcl -d "Tcl section"
complete -xc man -a n -d "New section"
complete -xc man -a l -d "Local section"
complete -xc man -a p
complete -xc man -a o -d "Old section"
complete -rc man -s C -d "Configuration file"
complete -xc man -s M -a "(__fish_complete_directory (commandline -ct))" -d "Manpath"
complete -rc man -s P -d "Pager"
complete -xc man -s S -d "Manual sections"
complete -c man -s a -d "Display all matches"
complete -c man -s c -d "Always reformat"
complete -c man -s d -d "Debug"
complete -c man -s D -d "Debug and run"
complete -c man -s f -d "Whatis"
complete -c man -s F -l preformat -d "Format only"
complete -c man -s h -d "Display help and exit"
complete -c man -s k -d "Apropos"
complete -c man -s K -d "Search in all man pages"
complete -xc man -s m -d "Set system"
complete -xc man -s p -d "Preprocessors"
complete -c man -s t -d "Format for printing"
complete -c man -s w -l path -d "Only print locations"
complete -c man -s W -d "Only print locations"

