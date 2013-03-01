
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

		apt-cache --no-generate pkgnames (commandline -tc)|sgrep -v \( |sgrep -v '\.so\(\.[0-9]\)*$'|sed -e 's/\/.*\///'|sed -e 's/$/'\t$package'/'
		return
	end

    # yum is slow, just like rpm, so go to the background
	if type -f /usr/share/yum-cli/completion-helper.py >/dev/null

		# If the cache is less than six hours old, we do not recalculate it

		set cache_file /tmp/.yum-cache.$USER
			if test -f $cache_file
			cat $cache_file
			set age (math (date +%s) - (stat -c '%Y' $cache_file))
			set max_age 21600
			if test $age -lt $max_age
				return
			end
		end

		# Remove package version information from output and pipe into cache file
        /usr/share/yum-cli/completion-helper.py list all -d 0 -C >$cache_file | cut -d '.' -f 1 | sed '1d' | sed '/^\s/d' | sed -e 's/$/'\t$package'/' &
	end

	# Rpm is too slow for this job, so we set it up to do completions
    # as a background job and cache the results.

	if type -f rpm >/dev/null

		# If the cache is less than five minutes old, we do not recalculate it

		set cache_file /tmp/.rpm-cache.$USER
			if test -f $cache_file
			cat $cache_file
			set age (math (date +%s) - (stat -c '%Y' $cache_file))
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

	# eix is MUCH faster than emerge so use it if it is available
	if type -f eix > /dev/null
		eix --only-names "^"(commandline -tc) | cut -d/ -f2
		return
	else
		# FIXME?  Seems to be broken
		if type -f emerge >/dev/null
			emerge -s \^(commandline -tc) |sgrep "^*" |cut -d\  -f3 |cut -d/ -f2
			return
		end
	end

end

