# Extract string between left and right separators of variable length.
# @params <left-sep> [<right-sep>] <string>
function msg.util.str.get
  set -l left   $argv[1]
  set -l right  $argv[1]
  set -l string $argv[2]

  if [ (count $argv) -gt 2 ]
    set right $argv[2]
    set string $argv[3]
  end

  set -l len (printf $left | awk '{print length}')
  # Match between outermost left / right separators.
  printf $string | sed "s/[^\\$left]*\(\\$left.*\\$right\)*/\1/g" | \
                   sed "s/^.\{$len\}\(.*\).\{$len\}\$/\1/"
end
