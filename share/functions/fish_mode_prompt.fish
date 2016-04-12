# The fish_mode_prompt function is prepended to the prompt
function fish_mode_prompt --description "Displays the current mode"
  # Do nothing if not in vi mode
  if test "$fish_key_bindings" = "fish_vi_key_bindings"
    switch $fish_bind_mode
      case default
        set_color --bold --background red white
        echo '[N]'
      case insert
        set_color --bold --background green white
        echo '[I]'
      case replace-one
        set_color --bold --background green white
        echo '[R]'
      case visual
        set_color --bold --background magenta white
        echo '[V]'
    end
    set_color normal
    echo -n ' '
  end
end
