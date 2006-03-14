
function __fish_print_packages

	# apt-cache is much, much faster than rpm, and can do this in real
    # time. We use it if available.

	switch (commandline -tc)
		case '-**'
			return
	end

	#Get the word 'Package' in the current language
	set -l package (_ Package)

	if type -f apt-cache >/dev/null
		# Apply the following filters to output of apt-cache:
		# 1) Remove package names with parentesis in them, since these seem to not correspond to actual packages as reported by rpm
		# 2) Remove package names that are .so files, since these seem to not correspond to actual packages as reported by rpm
		# 3) Remove path information such as /usr/bin/, as rpm packages do not have paths

		apt-cache --no-generate pkgnames (commandline -tc)|grep -v \( |grep -v '\.so\(\.[0-9]\)*$'|sed -e 's/\/.*\///'|sed -e 's/$/'\t$package'/'
		return
	end

	# Rpm is too slow for this job, so we set it up to do completions
    # as a background job and cache the results.

	if type -f rpm >/dev/null 

		# If the cache is less than five minutes old, we do not recalculate it

		set cache_file /tmp/.rpm-cache.$USER
			if test -f $cache_file 
			cat $cache_file
			set age (echo (date +%s) - (stat -c '%Y' $cache_file) | bc)
			set max_age 250
			if test $age -lt $max_age
				return
			end
		end

		# Remove package version information from output and pipe into cache file
		rpm -qa >$cache_file |sed -e 's/-[^-]*-[^-]*$//' | sed -e 's/$/'\t$package'/' &
	end

	# This completes the package name from the portage tree. 
	# True for installing new packages. Function for printing 
	# installed on the system packages is in completions/emerge.fish
	if type -f emerge >/dev/null
		emerge -s \^(commandline -tc) |grep "^*" |cut -d\  -f3 |cut -d/ -f2
		return
	end

end

