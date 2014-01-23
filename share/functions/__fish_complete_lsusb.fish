function __fish_complete_lsusb
	lsusb | awk '{print $2 ":" $4}'| cut -c1-7
end
