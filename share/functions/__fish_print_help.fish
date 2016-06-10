function __fish_print_help --description "Print help message for the specified fish function or builtin" --argument item
	# special support for builtin_help_get()
	set -l tty_width
	if test "$item" = "--tty-width"
		set tty_width $argv[2]
		set item $argv[3]
	end

	if test "$item" = '.'
		set item source
	end

	# Do nothing if the file does not exist
	if not test -e "$__fish_datadir/man/man1/$item.1" -o -e "$__fish_datadir/man/man1/$item.1.gz"
		return
	end

	set -l IFS \n\ \t

	# Render help output, save output into the variable 'help'
	set -l help
	set -l cols
	set -l rLL
	if test "$tty_width" -gt 0
		set cols $tty_width
	else if command test -t 1
		# We want to simulate `man`'s dynamic line length, because
		# defaulting to 80 kind of sucks.
		# Note: using `command test` instead of `test` because `test -t 1`
		# doesn't seem to work right.
		# Note: grab the size from the stdout terminal in case it's somehow
		# different than the stdin of fish.
		# use fd 3 to copy our stdout because we need to pipe the output of stty
		begin
			stty size 0<&3 | read __ cols
		end 3<&1
	end
	if test -n "$cols"
		set cols (math $cols - 4) # leave a bit of space on the right
		set rLL -rLL=$cols[1]n
	end
    if test -e "$__fish_datadir/man/man1/$item.1"
	    set help (nroff -man -c -t $rLL "$__fish_datadir/man/man1/$item.1" ^/dev/null)
    else if test -e "$__fish_datadir/man/man1/$item.1.gz"
	    set help (gunzip -c "$__fish_datadir/man/man1/$item.1.gz" ^/dev/null | nroff -man -c -t $rLL ^/dev/null)
    end

	# The original implementation trimmed off the top 5 lines and bottom 3 lines
	# from the nroff output. Perhaps that's reliable, but the magic numbers make
	# me extremely nervous. Instead, let's just strip out any lines that start
	# in the first column. "normal" manpages put all section headers in the first
	# column, but fish manpages only leave NAME like that, which we want to trim
	# away anyway.
	#
	# While we're at it, let's compress sequences of blank lines down to a single
	# blank line, to duplicate the default behavior of `man`, or more accurately,
	# the `-s` flag to `less` that `man` passes.
	set -l state blank
	for line in $help
		# categorize the line
		set -l line_type
		switch $line
		case ' *' \t\*
			# starts with whitespace, check if it has non-whitespace
			printf "%s\n" $line | read -l word __
			if test -n $word
				set line_type normal
			else
				# lines with just spaces probably shouldn't happen
				# but let's consider them to be blank
				set line_type blank
			end
		case ''
			set line_type blank
		case '*'
			# not leading space, and not empty, so must contain a non-space
			# in the first column. That makes it a header/footer.
			set line_type meta
		end

		switch $state
		case normal
			switch $line_type
			case normal
				printf "%s\n" $line
			case blank
				set state blank
			case meta
				# skip it
			end
		case blank
			switch $line_type
			case normal
				echo # print the blank line
				printf "%s\n" $line
				set state normal
			case blank meta
				# skip it
			end
		end
	end | ul # post-process with `ul`, to interpret the old-style grotty escapes
	echo # print a trailing blank line
end
