function __fish_man_page
	man (basename (commandline -po; echo)[1]) ^/dev/null
	or printf \a
end
