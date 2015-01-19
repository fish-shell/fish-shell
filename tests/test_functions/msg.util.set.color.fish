# Set style, fg/bg colors and reset. Modifies its parent scope of `msg`.
# @params [<fg>] [<bg>] [<style>]
function --no-scope-shadowing msg.util.set.color
  if [ (count $argv) -gt 0 ]
    set fg $argv[1]
  end

  if [ (count $argv) -gt 1 ]
    set bg $argv[2]
  end

  set_color -b $bg
  set_color    $fg

  if [ (count $argv) -gt 2 ]
    set_color $argv[3]
  end

  set bg $msg_color_bg
  set fg $msg_color_fg
end
