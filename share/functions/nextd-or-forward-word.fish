
function nextd-or-forward-word --key-binding 
	if test -z (commandline)
		nextd
	else
		commandline -f forward-word
	end
end
