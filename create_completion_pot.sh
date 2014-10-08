#!/bin/bash
transform_dir() {
	cd $1 #Begin transform
	mkdir transform original
	for i in *.fish
	do
		sed 's/.*\(--description.*\)/\1/' <"$i" >transform/"$i"
		mv "$i" original/
	done
	mv transform/*.fish ./
	cd - #End transform
	xgettext -j -k_ -kN_ -k--description -LShell --from-code=UTF-8 $1/*.fish -o messages.pot
	xgettext_status=$?
	cd $1 #Begin backup
	rm *.fish
	mv original/*.fish ./
	rmdir transform original
	cd - #End backup
	return $xgettext_status
}

transform_dir share/completions && transform_dir share/functions
