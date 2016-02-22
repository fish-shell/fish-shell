# True if substring exists in string.
# @params <substring> <string>
function msg.util.str.has
  printf $argv[2] | grep -q $argv[1]
end
