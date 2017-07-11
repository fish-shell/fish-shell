# Completions for passwd

function __fish_passwd_darwin_infosystem
	echo -e "PAM\tPluggable Authentication Module"
	echo -e "opendirectory\tOpen Directory-compatible system"
	echo -e "file\tLocal flat-file"
	echo -e "nis\tRemote NIS server"
end

if passwd --help >/dev/null ^&1
	complete -c passwd -n '__fish_contains_opt -s S status' -s a -l all -f --description "Display password state for all users"
	complete -c passwd -s d -l delete -f --description "Delete user password"
	complete -c passwd -s e -l expire -f --description "Immediately obsolete user password"
	complete -c passwd -s h -l help -f --description "Display help and exit"
	complete -c passwd -s i -l inactive -x --description "Schedule account inactivation after password expiration"
	complete -c passwd -s k -l keep-tokens -f --description "Wait tokens expiration before changing password"
	complete -c passwd -s l -l lock -f --description "Lock password"
	complete -c passwd -s n -l mindays -x --description "Define minimum delay between password changes"
	complete -c passwd -s q -l quiet -f --description "Be quiet"
	complete -c passwd -s r -l repository -r --description "Update given repository"
	complete -c passwd -s R -l root -r --description "Use given directory as working directory"
	complete -c passwd -s S -l status -f --description "Display account status"
	complete -c passwd -s u -l unlock -f --description "Unlock password"
	complete -c passwd -s w -l warndays -x --description "Define warning period before mandatory password change"
	complete -c passwd -s w -l warndays -x --description "Define maximum period of password validity"
	complete -c passwd -n '__fish_not_contain_opt -s A all' -f -a '(__fish_complete_users)' --description "Account to be altered"
else # Not Linux, so let's see what it is, with the ugly uname
	set os_type (uname)
	switch $os_type
		case Darwin # macOS family
			complete -c passwd -f -a '(__fish_complete_users)' --description "Account to be altered"
			complete -c passwd -x -s i -a '(__fish_passwd_darwin_infosystem)' --description "Directory system to apply the update to"
			complete -c passwd -x -s l --description "Location to be updated on chosen directory system"
			complete -c passwd -x -s u --description "User name to use on chosen directory system"
		case FreeBSD
			complete -c passwd -f -s l --description "Update locally, not in Kerberos"
			complete -c passwd -f -a '(__fish_complete_users)' --description "Account to be altered"
	end
	# This is common to Darwin, FreeBSD and OpenBSD, and is even the only possible completion under OpenBSD
	# This is separated from Linux completions, because the Linux one is useless with -A / --all,
	# whereas -A / --all does not exist under *BSD
	complete -c passwd -f -a '(__fish_complete_users)' --description "Account to be altered"
end
