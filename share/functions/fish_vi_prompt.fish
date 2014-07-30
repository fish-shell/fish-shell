function fish_vi_prompt_cm --description "Displays the current mode"
  echo -n " "
  switch $fish_bind_mode
    case default
      set_color --bold --background red white
      echo "[N]"
    case insert
      set_color --bold --background green white
      echo "[I]"
    case visual
      set_color --bold --background magenta white
      echo "[V]"
  end
  set_color normal
end

function fish_vi_prompt --description "Simple vi prompt"

	# Just calculate these once, to save a few cycles when displaying the prompt
	if not set -q __fish_prompt_hostname
		set -g __fish_prompt_hostname (hostname|cut -d . -f 1)
	end

	if not set -q __fish_prompt_normal
		set -g __fish_prompt_normal (set_color normal)
	end

	switch $USER

		case root toor

		if not set -q __fish_prompt_cwd
			if set -q fish_color_cwd_root
				set -g __fish_prompt_cwd (set_color $fish_color_cwd_root)
			else
				set -g __fish_prompt_cwd (set_color $fish_color_cwd)
			end
		end

		echo -n -s "$USER" @ "$__fish_prompt_hostname" ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal" (fish_vi_prompt_cm) '# '

		case '*'

		if not set -q __fish_prompt_cwd
			set -g __fish_prompt_cwd (set_color $fish_color_cwd)
		end

		echo -n -s "$USER" @ "$__fish_prompt_hostname" ' ' "$__fish_prompt_cwd" (prompt_pwd) "$__fish_prompt_normal" (fish_vi_prompt_cm) '> '

	end
end
