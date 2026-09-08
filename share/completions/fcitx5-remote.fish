# from fcitx5-remote --help
# Usage: fcitx5-remote [OPTION]
# 	-c		inactivate input method
# 	-o		activate input method
# 	-r		reload fcitx config
# 	-t,-T		switch Active/Inactive
# 	-e		Request fcitx to exit
# 	-a		print fcitx's dbus address
# 	-m <imname>	print corresponding addon name for im
# 	-g <group>	set current input method group
# 	-q		Get current input method group name
# 	-n		Get current input method name
# 	-s <imname>	switch to the input method uniquely identified by <imname>
# 	-x		Request fcitx to open a new X11 connection with the value of DISPLAY in the environment variable.
# 	--check		Check if Fcitx is already running, if not, return 1.
# 			Can be used with other options.
# 			The check will be done before sending DBus call to Fcitx.
# 			This can be used to send DBus call only when Fcitx is running.
# 	[no option]	display fcitx state, 0 for close, 1 for inactive, 2 for active
# 	-h		display this help and exit

complete -c fcitx5-remote -f
complete -c fcitx5-remote -s c -d "Inactivate input method"
complete -c fcitx5-remote -s o -d "Activate input method"
complete -c fcitx5-remote -s r -d "Reload config"
complete -c fcitx5-remote -s t -s T -d "Switch Active/Inactive"
complete -c fcitx5-remote -s e -d "Request fcitx to exit"
complete -c fcitx5-remote -s a -d "Print fcitx's dbus address"
complete -c fcitx5-remote -s m -x -d "Print corresponding addon name for im"
complete -c fcitx5-remote -s g -d "Set current input method group"
complete -c fcitx5-remote -s q -d "Get current input method group name"
complete -c fcitx5-remote -s n -d "Get current input method name"
complete -c fcitx5-remote -s s -d "Switch to the input method"
complete -c fcitx5-remote -s x -d "Request fcitx to open a new X11 connection with \$DISPLAY"
complete -c fcitx5-remote -l check -d "Check if fcitx is running (return 1 if not)"
complete -c fcitx5-remote -s h -l help -d "Display help message"
