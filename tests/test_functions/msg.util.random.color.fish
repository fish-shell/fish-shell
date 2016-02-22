# Print a random RRGGBB hexadecimal color from three min~max random beams
# where min = 0 and max = 255. Higher values produce lighter colors.
# @params [<min>][<max>]
function msg.util.random.color
  set -l min 0
  if [ (count $argv) -gt 0 ]
    set min $argv[1]
  end

  set -l max 255
  if [ (count $argv) -gt 1 ]
    set max $argv[2]
  end

  set beam "math (random)%\($max-$min+1\)+$min"
  printf "%02x%02x%02x" (eval $beam) (eval $beam) (eval $beam)
end
