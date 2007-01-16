#
# Completions for the shell interface to the KDE DCOP server
#

function __fish_complete_dcop -d "Completions for kde dcop"
    set -l com (commandline)

    if eval "$com >/dev/null 2>&1"
	# use dcop output if available
	eval "$com | sed -e 's/ (default)//; s/^\w[^ ]* \(\w*\)([^)]*)\$/\1/'"

    else
	set -l op (commandline -o)

	if test (count $op) -le 2
	    # select an application
	    dcop | grep -- $op[-1]
	else
	    # select a function
	    set -l o
	    for i in (seq 1 (count $op))
		# strip the \ character which causes problems
		set o[$i] (echo $op[$i] | sed -e 's/\\\//g')
	    end

	    set -l idx (seq 2 (expr (count $o) - 1))
	    # list only function names
	    dcop $o[$idx] | grep -- $o[-1] | sed -e 's/ (default)//; s/^\w[^ ]* \(\w*\)([^)]*)$/\1/'
	end
    end
end


complete -c dcop -l help -s h -f --description "Show help about options"
complete -c dcop -l user -x -a '(awk -F: "{print \$1}" /etc/passwd)' --description "Connect to the given user's DCOP server"
complete -c dcop -l all-users -f --description "Send the same DCOP call to all users with a running DCOP server"
complete -c dcop -l list-sessions -f --description "List all active KDE session for a user or all users"
complete -c dcop -l session -x -a '(dcop --list-sessions --all-users | grep DCOP)' --description "Send to the given KDE session"
complete -c dcop -l no-user-time -f --description "Don't update the user activity timestamp in the called application"
complete -c dcop -l pipe -f --description "Call DCOP for each line read from stdin"

complete -c dcop -x -a "(__fish_complete_dcop)"



