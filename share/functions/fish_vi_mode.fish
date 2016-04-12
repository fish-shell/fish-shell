function fish_vi_mode
	echo "This function is deprecated. Please call fish_vi_key_bindings directly" >&2
	# Turn on vi keybindings
	set -g fish_key_bindings fish_vi_key_bindings
end
