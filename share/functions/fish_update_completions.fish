function fish_update_completions --description "Update man-page based completions"
	eval $__fish_datadir/tools/create_manpage_completions.py --manpath --progress --yield-to $__fish_datadir/completions/
end
