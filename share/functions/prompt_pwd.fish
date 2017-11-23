function prompt_pwd --description "Print the current working directory, shortened to fit the prompt"
    set -l options 'h/help'
    argparse -n prompt_pwd --max-args=0 $options -- $argv
    or return

    if set -q _flag_help
        __fish_print_help prompt_pwd
        return 0
    end

    # This allows overriding fish_prompt_pwd_dir_length and fish_prompt_pwd_depth from the outside (global or universal) without leaking it
  set -q fish_prompt_pwd_dir_length
  or set -l fish_prompt_pwd_dir_length 1
  
  set -q fish_prompt_pwd_depth
  or set -l fish_prompt_pwd_depth 0

  # Replace $HOME with "~"
  set realhome ~
  set -l tmp (string replace -r '^'"$realhome"'($|/)' '~$1' $PWD)
  # Split $temp to an array(for the trim depth only) 
  set -l tempArr (string split '/' $tmp)

  if [ $fish_prompt_pwd_dir_length -eq 0 ]

    if [ $fish_prompt_pwd_depth -gt (count $tempArr) -o $fish_prompt_pwd_depth -eq 0 ] #dont trim, dont shorten
      echo $tmp 
    else #trim, dont shorten
      echo (string join '/' $tempArr[(math "-1 * "$fish_prompt_pwd_depth"")..-1]) 
    end

  else

    if [ $fish_prompt_pwd_depth -gt (count $tempArr) -o $fish_prompt_pwd_depth -eq 0 ] #dont trim, shorten
      # Shorten to at most $fish_prompt_pwd_dir_length characters per directory
      string replace -ar '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*/' '$1/' $tmp
    else #trim, shorten
      set -l trimmed_pwd (string join '/' $tempArr[(math "-1 * "$fish_prompt_pwd_depth"")..-1]) 
      string replace -ar '(\.?[^/]{'"$fish_prompt_pwd_dir_length"'})[^/]*/' '$1/' $trimmed_pwd
    end
    
  end
end
