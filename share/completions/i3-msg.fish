# i3-msg is a tool to send messages to the i3 window manager.
# See: https://i3wm.org

complete -c i3-msg -l quiet -s q -d 'Only send ipc message and suppress output'
complete -c i3-msg -l version -s v -d 'Display version number and exit'
complete -c i3-msg -l help -s h -d 'Display help and exit'
complete -c i3-msg -l socket -s s -d 'Set socket'
complete -c i3-msg -s t -x -d 'Specify ipc message type' -a '
  command\t"Payload is a command"
  get_workspaces\t"Get current workspace"
  get_outputs\t"Get current outputs"
  get_tree\t"Get layout tree"
  get_marks\t"Get list of marks"
  get_bar_config\t"Get list of configured binding modes"
  get_version\t"Get i3 version"
'
