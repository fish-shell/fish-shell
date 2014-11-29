function fish_save_colors --description 'Save current colors'
	begin
		echo "function fish_load_colors --description 'Restore colors'"
		for color in (set -n | grep color)
			echo set $color $$color
		end
		echo end
	end | fish_indent | source
	funcsave fish_load_colors
end
