# General kitty
complete -c kitty -f
complete -c kitty -s h -l help -d 'Show help'

# Kitty options
complete -c kitty -l class -d 'Set the class part of the WM_CLASS property' -n 'not __kitty_is_message'
complete -c kitty -l title -d 'Set the window title' -n 'not __kitty_is_message'
complete -c kitty -l config -d 'Specify a path to the configuration file(s) to use' -n 'not __kitty_is_message'
complete -c kitty -x -s o -l override -d 'Override individual configuration option' -a "(__kitty_config)" -n 'not __kitty_is_message'
complete -c kitty -s c -l cmd -d 'Run python code in the kitty context' -f -n 'not __kitty_is_message'
complete -c kitty -s d -l directory -d 'Change to specified directory when launching' -n 'not __kitty_is_message'
complete -c kitty -l detach -d 'Detach from the controlling terminal, if any' -n 'not __kitty_is_message'
complete -c kitty -l window-layout -d 'The window layout to use on startup' -n 'not __kitty_is_message' -xa 'tall grid vertical stack horizontal'
complete -c kitty -r -l session -d 'Path to a file containing the startup session' -n 'not __kitty_is_message'
complete -c kitty -s 1 -l single-instance -d 'Run only a single instance of kitty' -n 'not __kitty_is_message'
complete -c kitty -x -l instance-group -d 'Create window within instance group' -n 'not __kitty_is_message; and __fish_contains_opt -s 1 single-instance'
complete -c kitty -l listen-on -d 'Tell kitty to listen on the specified address for control messages' -n 'not __kitty_is_message'

# Kitty message commands
complete -c kitty -x -n '__kitty_needs_subcommand' -d 'Ensure allow_remote_control yes is set' -a "
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
complete -c kitty -x -l to -d 'An address for the kitty instance to control' -n '__kitty_needs_subcommand'

# match
complete -c kitty -x -s m -l match -d 'The tab to match. Match specifications are of the form: field:regexp' -n '__kitty_is_message; and not __kitty_needs_subcommand; and not __kitty_is_cmd ls'

# self
complete -c kitty -l self -d 'Close the tab this command is run in, rather than the active tab' -n '__kitty_is_message; and __kitty_is_cmd close-window; or __kitty_is_cmd close-tab; or __kitty_is_cmd get-text'

# send-text
complete -c kitty -l stdin -d 'Read the text to be sent from stdin' -n '__kitty_is_message; and __kitty_is_cmd send-text'
complete -c kitty -r -l from-file -d 'Read the text to be sent from stdin' -n '__kitty_is_message; and __kitty_is_cmd send-text'

# get-text
complete -c kitty -x -l extent -d 'What text to get' -n '__kitty_is_message; and __kitty_is_cmd get-text' -a "
  screen\t'All text currently on the screen (default)'
  selection\t'Currently selected text'
  all\t'All the screen and scrollback'
"
complete -c kitty -l ansi -d 'Include formatting escape codes' -n '__kitty_is_message; and __kitty_is_cmd get-text'

# new-window
complete -c kitty -x -l title -d 'The title of the new window' -n '__kitty_is_message; and __kitty_is_cmd new-window; and __fish_not_contain_opt new-tab'
complete -c kitty -r -l cwd -d 'The initial working directory for the new window' -n '__kitty_is_message; and __kitty_is_cmd new-window'
complete -c kitty -r -l new-tab -d 'Open a new tab' -n '__kitty_is_message; and __kitty_is_cmd new-window'
complete -c kitty -x -l tab-title -d 'The title of the tab' -n '__kitty_is_message; and __kitty_is_cmd new-window; and __fish_contains_opt new-tab'


# Helper functions
function __kitty_config
  set -l candidate ~/.config/kitty
  if set -q KITTY_CONFIG_DIRECTORY
    set candidate "$KITTY_CONFIG_DIRECTORY"
  else if set -q XDG_CONFIG_HOME
    set candidate "$XDG_CONFIG_HOME"/kitty
  else if test (uname) = "Darwin"
    set candidate ~/Library/Preferences
  end

  set candidate $candidate/kitty.conf

  if not test -e $candidate
    return
  end

  cat $candidate | grep -P "^[^#]" | grep -P "^(?!map)" | awk '{print $1"\t"$2}'
end

function __kitty_is_message
  commandline | string match -qr "^kitty\W+@"
end

function __kitty_needs_subcommand
  commandline | string -qr '^kitty\W+@\W+[a-z-]*$'
end

function __kitty_is_cmd
  commandline | string -qr "^kitty\W+@\W+$argv[1]"
end
