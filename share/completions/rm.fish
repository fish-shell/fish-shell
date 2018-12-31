#Completions for rm
if rm --version >/dev/null 2>/dev/null # GNU
	complete -c rm -s d -l directory -d "Unlink directory (Only by superuser)"
	complete -c rm -s f -l force -d "Never prompt before removal"
	complete -c rm -s i -l interactive -d "Prompt before removal"
	complete -c rm -s I -d "Prompt before removing more than three files"
	complete -c rm -s r -l recursive -d "Recursively remove subdirectories"
	complete -c rm -s R -d "Recursively remove subdirectories"
	complete -c rm -s v -l verbose -d "Explain what is done"
	complete -c rm -s h -l help -d "Display help and exit"
	complete -c rm -l version -d "Display version and exit"
else
	set -l uname (uname -s)
	# solaris:   rm [-fi        ] file ...
	# openbsd:   rm [-fidPRrv   ] file ...
	# macos:     rm [-fidPRrvW  ] file ...
	# netbsd:    rm [-fidPRrvWx ] file ...
	# freebsd:   rm [-fidPRrvWxI] file ...
	# dragonfly: rm [-fidPRrvWxI] file ... 

	complete -c rm -s f -d "Never prompt before removal"
	complete -c rm -s i -d "Prompt before removal"
	test "$uname" = SunOS
		and exit 0
	complete -c rm -s d -d "Attempt to remove directories as well"
	complete -c rm -s P -d "Overwrite before removal"
	complete -c rm -s R -d "Recursively remove subdirectories"
	complete -c rm -s r -d "Recursively remove subdirectories"
	complete -c rm -s v -d "Explain what is done"
	test "$uname" = OpenBSD
		and exit 0
	complete -c rm -s W -d "Undelete the named files"
	test "$uname" = Darwin
		and exit 0
	complete -c rm -s x -d "Do not traverse filesystem mount points"
	test "$uname" = NetBSD
		and exit 0
	complete -c rm -s I -d "Like -i, but only if 3 or more files affected"
end


