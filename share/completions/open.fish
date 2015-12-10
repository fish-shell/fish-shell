if test (uname) = 'Darwin' # OS X
	complete -c open -s a -d 'Open APP, or open FILE(s), if supplied, with APP' -x -a "(mdfind -onlyin /Applications -onlyin ~/Applications -onlyin /Developer/Applications 'kMDItemKind==Application' | sed -E 's/.+\/(.+)\.app/\1/g')"
	complete -c open -s b -d 'Bundle Identifier of APP to open, or to be used to open FILE' -x -a "(mdls (mdfind -onlyin /Applications -onlyin ~/Applications -onlyin /Developer/Applications 'kMDItemKind==Application') -name kMDItemCFBundleIdentifier | sed -E 's/kMDItemCFBundleIdentifier = \"(.+)\"/\1/g')"
	complete -c open -s e -d 'Open FILE(s) with /Applications/TextEdit'
	complete -c open -s t -d 'Open FILE(s) with default text editor from LaunchServices'
	complete -c open -s f -d 'Open STDIN/PIPE with default text editor. End input with C-d.'
	complete -c open -s F -d 'Don\'t restore previous application windows. Except Untitled documents.'
	complete -c open -s W -d 'Wait until APP has exited'
	complete -c open -s R -d 'Reveal FILE(s) in Finder'
	complete -c open -s n -d 'Open new instace of APP'
	complete -c open -s g -d 'Open in background'
	complete -c open -s h -d 'Finds and opens for a header whose name matches the given string. Better performance with full names i.e. NSView.h' -r
	complete -c open -l args -d 'Pass arguments to opened APP in the argv parameter to main()' -x
end
