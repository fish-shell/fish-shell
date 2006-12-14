
function nextd-or-forward-word
	if test -z (commandline)
		nextd
		commandline -f repaint
	else
		commandline -f forward-word
	end
end
