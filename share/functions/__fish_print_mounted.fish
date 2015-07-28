function __fish_print_mounted --description 'Print mounted devices'
	if test -r /etc/mtab
		# In mtab, spaces are replaced by a literal '\040'
		# So it's safe to get the second "field" and then replace it
		sed -e "s/[^ ]\+ \([^ ]\+\) .*/\\1/" -e "s/\\040/ /g" /etc/mtab
	else
		mount | cut -d " " -f 1-3|tr " " \n|sed -e "s/[0-9\.]*:\//\//"| __fish_sgrep "^/"
	end
end
