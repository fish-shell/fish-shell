# General kitty
complete -c kitty -f
complete -c kitty -s h -l help -d 'Show help'

# Kitty options
complete -c kitty -l class -d 'Set the class part of the WM_CLASS property' -n 'not __kitty_is_message'
complete -c kitty -l title -d 'Set the window title' -n 'not __kitty_is_message'
complete -c kitty -l config -d 'Specify a path to the configuration file(s) to use' -n 'not __kitty_is_message'
complete -c kitty -s o -l override -d 'Override individual configuration option' -xa "(__kitty_config)" -n 'not __kitty_is_message'
complete -c kitty -s c -l cmd -d 'Run python code in the kitty context' -f -n 'not __kitty_is_message'
complete -c kitty -s d -l directory -d 'Change to specified directory when launching' -n 'not __kitty_is_message'
complete -c kitty -l detach -d 'Detach from the controlling terminal, if any' -n 'not __kitty_is_message'
complete -c kitty -l window-layout -d 'The window layout to use on startup' -n 'not __kitty_is_message' -xa 'tall grid vertical stack horizontal'
complete -c kitty -l session -d 'Path to a file containing the startup session' -rn 'not __kitty_is_message'
complete -c kitty -s 1 -l single-instance -d 'Run only a single instance of kitty' -n 'not __kitty_is_message'
complete -c kitty -l instance-group -d 'Create window within instance group' -xn 'not __kitty_is_message; and __fish_contains_opt -s 1 single-instance'

# Kitty message commands
complete -c kitty -n '__kitty_needs_subcommand' -d 'Ensure allow_remote_control yes is set' -xa "
  close-tab\t'Close the specified tab(s)'
  close-window\t'Close the specified window(s)'
  focus-window\t'Focus the specified window'
  focus-tab\t'Focus the specified tab'
  get-text\t'Get text from the specified window'
  ls\t'List all tabs/windows'
  new-window\t'Open new window'
  send-text\t'Send arbitrary text to specified windows'
  set-tab-title\t'Set the tab title'
  set-window-title\t'Set the window title'
"

# match
complete -c kitty -s m -l match -d 'The tab to match. Match specifications are of the form: field:regexp' -n '__kitty_is_message; and not __kitty_needs_subcommand; and not __kitty_is_cmd ls'

# self
complete -c kitty -l self -d 'Close the tab this command is run in, rather than the active tab' -n '__kitty_is_message; and __kitty_is_cmd close-window; or __kitty_is_cmd close-tab; or __kitty_is_cmd get-text'

# send-text
complete -c kitty -l stdin -d 'Read the text to be sent from stdin' -xn '__kitty_is_message; and __kitty_is_cmd send-text'
complete -c kitty -l from-file -d 'Read the text to be sent from stdin' -rn '__kitty_is_message; and __kitty_is_cmd send-text'

# get-text
complete -c kitty -l extent -d 'What text to get' -n '__kitty_is_message; and __kitty_is_cmd get-text' -xa "
  screen\t'All text currently on the screen (default)'
  selection\t'Currently selected text'
  all\t'All the screen and scrollback'
"
complete -c kitty -l ansi -d 'Include formatting escape codes' -xn '__kitty_is_message; and __kitty_is_cmd get-text'

# new-window
complete -c kitty -l title -d 'The title of the new window' -xn '__kitty_is_message; and __kitty_is_cmd new-window; and __fish_not_contain_opt new-tab'
complete -c kitty -l cwd -d 'The initial working directory for the new window' -rn '__kitty_is_message; and __kitty_is_cmd new-window'
complete -c kitty -l new-tab -d 'Open a new tab' -rn '__kitty_is_message; and __kitty_is_cmd new-window'
complete -c kitty -l tab-title -d 'The title of the tab' -xn '__kitty_is_message; and __kitty_is_cmd new-window; and __fish_contains_opt new-tab'


# Helper functions
function __kitty_config
  cat $HOME/.config/kitty/kitty.conf | grep -P "^[^#]" | grep -P "^(?!map)" | awk '{print $1}'
end

function __kitty_is_message
  test (commandline | grep "@")
end

function __kitty_needs_subcommand
  test (commandline | grep -P '@ [a-z-]*$')
end

function __kitty_is_cmd
  test (commandline | grep "$argv[1]")
end
