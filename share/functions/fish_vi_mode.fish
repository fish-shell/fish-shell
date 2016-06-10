function fish_vi_mode
	echo 'The `fish_vi_mode` function is deprecated.' >&2
        echo 'Please switch to calling `fish_vi_key_bindings`.' >&2
	# Turn on vi keybindings
	set -g fish_key_bindings fish_vi_key_bindings
end
