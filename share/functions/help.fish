function help --description 'Show help for the fish shell'
	
	# Declare variables to set correct scope
	set -l fish_browser
	set -l fish_browser_bg

	set -l h syntax completion editor job-control todo bugs history killring help
	set h $h color prompt title variables builtin-overview changes expand
	set h $h expand-variable expand-home expand-brace expand-wildcard
	set -l help_topics $h expand-command-substitution expand-process

	# 'help -h' should launch 'help help'
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help help
				return 0
		end
	end

	#
	# Find a suitable browser for viewing the help pages. This is needed
	# by the help function defined below.
	#
	set -l graphical_browsers htmlview x-www-browser firefox galeon mozilla konqueror epiphany opera netscape rekonq google-chrome chromium-browser
	set -l text_browsers htmlview www-browser links elinks lynx w3m

	if type -q "$BROWSER"
		# User has manually set a preferred browser, so we respect that
		set fish_browser $BROWSER

		# If browser is known to be graphical, put into background
		if contains -- $BROWSER $graphical_browsers
			set fish_browser_bg 1
		end
	else
		# Check for a text-based browser.
		for i in $text_browsers
			if type -q -f $i
				set fish_browser $i
				break
			end
		end

		# If we are in a graphical environment, check if there is a graphical
		# browser to use instead.
		if test "$DISPLAY" -a \( "$XAUTHORITY" = "$HOME/.Xauthority" -o "$XAUTHORITY" = "" \)
			for i in $graphical_browsers
				if type -q -f $i
					set fish_browser $i
					set fish_browser_bg 1
					break
				end
			end
		end

		# If the OS appears to be Windows (graphical), try to use cygstart
		if type -q cygstart
			set fish_browser cygstart
		# If xdg-open is available, just use that
		else if type -q xdg-open
			set fish_browser xdg-open
		end
	
	
		# On OS X, we go through osascript by default
		if test (uname) = Darwin
			if type -q osascript
				set fish_browser osascript
			end
		end

	end

	if test -z $fish_browser
		printf (_ '%s: Could not find a web browser.\n') help
				printf (_ 'Please set the variable $BROWSER to a suitable browser and try again.\n\n')
		return 1
	end

	# In Cygwin, start the user-specified browser using cygstart
	if type -q cygstart
		if test $fish_browser != "cygstart"
			# Escaped quotes are necessary to work with spaces in the path
			# when the command is finally eval'd.
			set fish_browser cygstart \"$fish_browser\"
		end
	end

	set -l fish_help_item $argv[1]

	switch "$fish_help_item"
		case ""
			set fish_help_page index.html
		case "."
			set fish_help_page "commands.html\#source"
		case globbing
			set fish_help_page "index.html\#expand"
		case (__fish_print_commands)
			set fish_help_page "commands.html\#$fish_help_item"
		case $help_topics
			set fish_help_page "index.html\#$fish_help_item"
		case "*"
			if type -q -f $fish_help_item
				# Prefer to use fish's man pages, to avoid
				# the annoying useless "builtin" man page bash
				# installs on OS X
				set -l man_arg "$__fish_datadir/man/man1/$fish_help_item.1"
				if test -f "$man_arg"
					man $man_arg
					return
				end
			end
			set fish_help_page "index.html"
	end
	
	set -l page_url
	if test -f $__fish_help_dir/index.html
		# Help is installed, use it
		set page_url file://$__fish_help_dir/$fish_help_page

		# In Cygwin, we need to convert the base help dir to a Windows path before converting it to a file URL
		if type -q cygpath
			set page_url file://(cygpath -m $__fish_help_dir)/$fish_help_page
		end
	else
		# Go to the web. Only include one dot in the version string
		set -l version_string (echo $FISH_VERSION| cut -d . -f 1,2)
		set page_url http://fishshell.com/docs/$version_string/$fish_help_page
	end
	
	# OS X /usr/bin/open swallows fragments (anchors), so use osascript
	# Eval is just a cheesy way of removing the hash escaping
	if test "$fish_browser" = osascript
		osascript -e 'open location "'(eval echo $page_url)'"'
		return
	end
	
	if test $fish_browser_bg

		switch $fish_browser
			case 'htmlview' 'x-www-browser'
				printf (_ 'help: Help is being displayed in your default browser.\n')

			case '*'
				printf (_ 'help: Help is being displayed in %s.\n') $fish_browser

		end

		eval "$fish_browser $page_url &"
	else
		eval $fish_browser $page_url
	end
end
