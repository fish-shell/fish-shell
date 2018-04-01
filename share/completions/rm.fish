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
else # OSX
	complete -c rm -s d -d "Attempt to remove directories as well"
	complete -c rm -s f -d "Never prompt before removal"
	complete -c rm -s i -d "Prompt before removal"
	complete -c rm -s P -d "Overwrite before removal"
	complete -c rm -s r -d "Recursively remove subdirectories"
	complete -c rm -s R -d "Recursively remove subdirectories"
	complete -c rm -s v -d "Explain what is done"
	complete -c rm -s W -d "Undelete the named files"
end


