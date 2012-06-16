function __fish_complete_proc --description 'Complete by list of running processes'
	ps -A --no-headers --format comm | sort -u

end
