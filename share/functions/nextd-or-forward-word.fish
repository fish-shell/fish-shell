
function nextd-or-forward-word
	if test -z (commandline)
		nextd
	else
		commandline -f forward-word
	end
end
