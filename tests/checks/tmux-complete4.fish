#RUN: %fish %s
#REQUIRES: command -v tmux
#REQUIRES: uname -r | grep -qv Microsoft

isolated-tmux-start -C '
    complete : -s c -l clip
    complete : -s q -l qrcode
    set -g fish_autosuggestion_enabled 0
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
