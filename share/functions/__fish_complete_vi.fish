#completion for vi/vim

function __fish_complete_vi -d "Completions for vi and its aliases" --argument-names cmd
	set -l is_vim
	if type -q $cmd
		eval command $cmd --version >/dev/null ^/dev/null; and set -l is_vim vim
	end

	# vim
	set -l cmds -c $cmd
	if test -n "$is_vim"
		# these don't work
		#complete $cmds -a - --description 'The file to edit is read from stdin. Commands are read from stderr, which should be a tty'

		# todo
		# +[num]        : Position the cursor on line number
		# +/{pat}       : Position the cursor on the first occurence of {pat}
		# +{command}    : Execute Ex command after the first file has been read
		complete $cmds -s c -r --description 'Execute Ex command after the first file has been read'
		complete $cmds -s S -r --description 'Source file after the first file has been read'
		complete $cmds -l cmd -r --description 'Execute Ex command before loading any vimrc'
		complete $cmds -s d -r --description 'Use device as terminal (Amiga only)'
		complete $cmds -s i -r --description 'Set the viminfo file location'
		complete $cmds -s o -r --description 'Open stacked windows for each file'
		complete $cmds -s O -r --description 'Open side by side windows for each file'
		complete $cmds -s p -r --description 'Open tab pages for each file'
		complete $cmds -s q -r --description 'Start in quickFix mode'
		complete $cmds -s r -r --description 'Use swap files for recovery'
		complete $cmds -s s -r --description 'Source and execute script file'
		complete $cmds -s t -r --description 'Set the cursor to tag'
		complete $cmds -s T -r --description 'Terminal name'
		complete $cmds -s u -r --description 'Use alternative vimrc'
		complete $cmds -s U -r --description 'Use alternative vimrc in GUI mode'
		complete $cmds -s w -r --description 'Record all typed characters'
		complete $cmds -s W -r --description 'Record all typed characters (overwrite file)'

		complete $cmds -s A --description 'Start in Arabic mode'
		complete $cmds -s b --description 'Start in binary mode'
		complete $cmds -s C --description 'Behave mostly like vi'
		complete $cmds -s d --description 'Start in diff mode'
		complete $cmds -s D --description 'Debugging mode'
		complete $cmds -s e --description 'Start in Ex mode'
		complete $cmds -s E --description 'Start in improved Ex mode'
		complete $cmds -s f --description 'Start in foreground mode'
		complete $cmds -s F --description 'Start in Farsi mode'
		complete $cmds -s g --description 'Start in GUI mode'
		complete $cmds -s h --description 'Print help message and exit'
		complete $cmds -s H --description 'Start in Hebrew mode'
		complete $cmds -s L --description 'List swap files'
		complete $cmds -s l --description 'Start in lisp mode'
		complete $cmds -s m --description 'Disable file modification'
		complete $cmds -s M --description 'Disallow file modification'
		complete $cmds -s N --description 'Reset compatibility mode'
		complete $cmds -s n --description 'Don\'t use swap files'
		complete $cmds -s R --description 'Read only mode'
		complete $cmds -s r --description 'List swap files'
		complete $cmds -s s --description 'Start in silent mode'
		complete $cmds -s V --description 'Start in verbose mode'
		complete $cmds -s v --description 'Start in vi mode'
                complete $cmds -s x --description 'Use encryption when writing files'
		complete $cmds -s X --description 'Don\'t connect to X server'
		complete $cmds -s y --description 'Start in easy mode'
		complete $cmds -s Z --description 'Start in restricted mode'

		complete $cmds -o nb --description 'Become an editor server for NetBeans'

		complete $cmds -l no-fork --description 'Start in foreground mode'
		complete $cmds -l echo-wid --description 'Echo the Window ID on stdout (GTK GUI only)'
		complete $cmds -l help --description 'Print help message and exit'
		complete $cmds -l literal --description 'Do not expand wildcards'
		complete $cmds -l noplugin --description 'Skip loading plugins'
		complete $cmds -l remote --description 'Edit files on Vim server'
		complete $cmds -l remote-expr --description 'Evaluate expr on Vim server'
		complete $cmds -l remote-send --description 'Send keys to Vim server'
		complete $cmds -l remote-silent --description 'Edit files on Vim server'
		complete $cmds -l remote-wait --description 'Edit files on Vim server'
		complete $cmds -l remote-wait-silent --description 'Edit files on Vim server'
		complete $cmds -l serverlist --description 'List all Vim servers that can be found'
		complete $cmds -l servername --description 'Set server name'
		complete $cmds -l version --description 'Print version information and exit'

		complete $cmds -l socketid -r --description 'Run gvim in another window (GTK GUI only)'

	# plain vi (as bundled with SunOS 5.8)
	else

		# todo:
		# -wn : Set the default window size to n
		# +command : same as -c command

		complete $cmds -s s --description 'Suppress all interactive user feedback'
		complete $cmds -s C --description 'Encrypt/decrypt text'
		complete $cmds -s l --description 'Set up for editing LISP programs'
		complete $cmds -s L --description 'List saved file names after crash'
                complete $cmds -s R --description 'Read-only mode'
		complete $cmds -s S --description 'Use linear search for tags if tag file not sorted'
		complete $cmds -s v --description 'Start in display editing state'
		complete $cmds -s V --description 'Verbose mode'
		complete $cmds -s x --description 'Encrypt/decrypt text'

		complete $cmds -r -s r --description 'Recover file after crash'
		complete $cmds -r -s t --description 'Edit the file containing a tag'
		complete $cmds -r -c t --description 'Begin editing by executing the specified  editor command'

	end

end


