# mqudsi: Given the size and scope of curl's arguments, I don't have the time
# to add proper completions, but want to enable path completion for data file
# parameters, which allow specifying the path to a payload to upload as @path,
# which fish won't complete otherwise.

complete -c curl -n 'string match -qr "^@" -- (commandline -ct)' -xa "(printf '%s\n' -- @(__fish_complete_suffix (commandline -ct | string replace -r '^@' '') ''))"
