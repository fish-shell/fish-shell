function dirs -d (_ "Print directory stack")
	echo -n (command pwd)"  "
	for i in $dirstack
		echo -n $i"  "
	end
	echo
end


