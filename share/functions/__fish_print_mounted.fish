function __fish_print_mounted --description 'Print mounted devices'
	if test (uname) = Darwin
		mount | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|sgrep "^/"
	else
		cat /etc/mtab | cut -d " " -f 1-2|tr " " \n|sed -e "s/[0-9\.]*:\//\//"|sgrep "^/"
	end
end
