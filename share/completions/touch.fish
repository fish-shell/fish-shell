if touch --version 2>/dev/null > /dev/null # GNU
	complete -c touch -s a -d "Change access time"
	complete -c touch -s B -l backward -x -d "Set date back"
	complete -c touch -s c -l no-create -d "Do not create file"
	complete -c touch -s d -l date -x -d "Set date"
	complete -c touch -s f -l forward -x -d "Set date forward"
	complete -c touch -s m -d "Change modification time"
	complete -c touch -s r -l reference -d "Use this files times"
	complete -c touch -s t -d "Set date"
	complete -c touch -l time -x -d "Set time"
	complete -c touch -l help -d "Display help and exit"
	complete -c touch -l version -d "Display version and exit"
else # OS X
	complete -c touch -s A -d "Adjust access and modification time stamps by specified VALUE" -r
	complete -c touch -s a -d "Change access time of file"
	complete -c touch -s c -d "Don't create file if it doesn't exist"
	complete -c touch -s f -d "Attempt to force the update, even when permission don't permit"
	complete -c touch -s h -d "Change times of the symlink ranther than the file. Implies `-c'"
	complete -c touch -s m -d "Change modification time of file"
	complete -c touch -s r -d "Use access and modifications times from specified file rather than current time of day"
	complete -c touch -s t -d "Change access and modifications times to specified file rather than current time of day"
end