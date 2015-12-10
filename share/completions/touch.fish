if touch --version ^ /dev/null > /dev/null # GNU
	complete -c touch -s a --description "Change access time"
	complete -c touch -s B -l backward -x --description "Set date back"
	complete -c touch -s c -l no-create --description "Do not create file"
	complete -c touch -s d -l date -x --description "Set date"
	complete -c touch -s f -l forward -x --description "Set date forward"
	complete -c touch -s m --description "Change modification time"
	complete -c touch -s r -l reference --description "Use this files times"
	complete -c touch -s t --description "Set date"
	complete -c touch -l time -x --description "Set time"
	complete -c touch -l help --description "Display help and exit"
	complete -c touch -l version --description "Display version and exit"
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