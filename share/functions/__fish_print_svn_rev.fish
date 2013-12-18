function __fish_print_svn_rev --description 'Print svn revisions'
	svn info | grep "Last Changed Rev" | cut -d " " -f 4
	echo \{\tRevision at start of the date
	echo HEAD\tLatest in repository
	echo BASE\tBase rev of item\'s working copy
	echo COMMITTED\tLatest commit at or befor base
	echo PREV\tRevision just before COMMITTED


end
