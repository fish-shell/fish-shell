function __fish_complete_proc --description 'Complete by list of running processes'
	ps a --no-headers --format comm | uniq

end
