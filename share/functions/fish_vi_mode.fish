function fish_vi_mode
  # Set the __fish_vi_mode variable
  # This triggers fish_mode_prompt to output the mode indicator
  set -g __fish_vi_mode 1
  
  # Turn on vi keybindings
  set -g fish_key_bindings fish_vi_key_bindings
end
