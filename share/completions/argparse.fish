function __argparse_find_option_specs --description 'Internal function to find all option specs'
  set --local delimiter ','
  set --local cmd (commandline --tokenize)
  set --erase cmd[1]

  if set index (contains --index -- '--' $cmd)
    set index (math $index - 1)
  else
    set index (count $cmd)
  end

  set --local specs
  while test $index -gt 0
    and not string match --quiet -- '-*' $cmd[$index]
    if test $index -gt 1
      and string match --regex --quiet -- '^(-x|--exclusive)$' $cmd[(math $index - 1)]
      and ! test -z (commandline --current-token)
      break
    end

    if string match --quiet '*=*' $cmd[$index]
      set cmd[$index] (string replace --regex '=.*' '' $cmd[$index])
    else if string match --quiet '*=\?*' $cmd[$index]
      set cmd[$index] (string replace --regex '=\?.*' '' $cmd[$index])
    else if string match --quiet '*=+*' $cmd[$index]
      set cmd[$index] (string replace --regex '=\+*' '' $cmd[$index])
    end

    if string match --quiet '*/*' $cmd[$index]
      set --append specs (string split '/' $cmd[$index])
    else
      set --append specs $cmd[$index]
    end
    set index (math $index - 1)
  end

  echo -n $specs | string replace --regex --all ' ' '\n'
end

set --local CONDITION '! __fish_seen_argument --short r --long required-val --short o --long optional-val'

complete --command argparse --no-files

complete --command argparse --short-option h --long-option help --description 'Show help'

complete --command argparse --short-option n --long-option name --require-parameter --no-files --arguments '(functions --all | string replace ", " "\n")' --description 'Use function name'
complete --command argparse --short-option x --long-option exclusive --no-files --require-parameter --arguments '(__fish_append "," (__argparse_find_option_specs))' --description 'Specify mutually exclusive options'
complete --command argparse --short-option N --long-option min-args --description 'Specify minimum non-option argument count'
complete --command argparse --short-option X --long-option max-args --description 'Specify maximum non-option argument count'
complete --command argparse --short-option i --long-option ignore-unknown --description 'Ignore unknown options'
complete --command argparse --short-option s --long-option stop-nonopt --description 'Exit on subcommand'
