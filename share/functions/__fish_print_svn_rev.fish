function __fish_print_svn_rev --description 'Print svn revisions'
	printf '%s' (svnversion | sed 's=[^0-9:]*==g')
end
