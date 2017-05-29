function protocol --description 'Determine URI protocol, if any, in a string.'
  switch $argv; case '*://*'; echo $argv | sed -e 's/\:\/\/.*//'; end
end