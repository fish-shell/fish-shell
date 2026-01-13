#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft

isolated-tmux-start -C '
    set -g fish_autosuggestion_enabled 0
    complete : -s c -l clip
    complete : -s q -l qrcode
    complete true -l profile
    complete true -l ProfileManager
'
touch somefile1
touch somefile2

isolated-tmux send-keys C-l ': -c'

function tab
    isolated-tmux send-keys Tab
    tmux-sleep
    isolated-tmux capture-pane -p | awk '/./ { print "[" $0 "]" }'
end

tab
# CHECK: [prompt 0> : -cq]
tab
# CHECK: [prompt 0> : -cq somefile]
# CHECK: [somefile1  somefile2]
tab
# CHECK: [prompt 0> : -cq somefile1]
# CHECK: [somefile1  somefile2]

isolated-tmux send-keys C-u C-l 'true --pro'
tab
# CHECK: [prompt 0> true --profile]
# CHECK: [--profile  --ProfileManager]
tab
# CHECK: [prompt 0> true --profile]
# CHECK: [--profile  --ProfileManager]
isolated-tmux send-keys foo
tmux-sleep
isolated-tmux capture-pane -p | awk '/./ { print "[" $0 "]" }'
# CHECK: [prompt 0> true --profile foo]
