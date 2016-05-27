function __fish_complete_blockdevice
	set -l cmd (commandline -ct)
	[ "" = "$cmd" ]; and return
	for f in $cmd*
		[ -b $f ]; and printf "%s\t%s\n" $f "Block device"
		[ -d $f ]; and printf "%s\n" $f/
	end
end
