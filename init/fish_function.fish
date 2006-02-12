
#
# Print the current working directory in a shortened form. This
# function is used by the default prompt command.
#

function prevd-or-backward-word --key-binding 
	if test -z (commandline)
		prevd
	else
		commandline -f backward-word
	end
end

function nextd-or-forward-word --key-binding 
	if test -z (commandline)
		nextd
	else
		commandline -f forward-word
	end
end

#
# This function deletes a character from the commandline if it is
# non-empty, and exits the shell otherwise. Implementing this
# functionality has been a longstanding request from various
# fish-users.
#

function delete-or-exit --key-binding 
	if test (commandline)
		commandline -f delete-char
	else
		exit
	end
end

