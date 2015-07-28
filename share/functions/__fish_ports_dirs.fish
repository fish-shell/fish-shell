# a function to print a list of ports local collections

function __fish_ports_dirs -d 'Obtain a list of ports local collections'
	for f in /usr/ports/*
		echo $f
	end
end
