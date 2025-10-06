# localization: skip(private)
set -l erase_to_end_of_screen "$(
    if status test-feature ignore-terminfo
        echo \e\[J
    else if type -q tput
        tput ed
    end
)"

function __fish_echo --inherit-variable erase_to_end_of_screen --description 'run the given command after the current commandline and redraw the prompt'
    set -l line (commandline --line)
    string >&2 repeat -N \n --count=(math (commandline | count) - $line + 1)
    printf %s $erase_to_end_of_screen >&2
    $argv >&2
    string >&2 repeat -N \n --count=(math (count (fish_prompt)) - 1)
    string >&2 repeat -N \n --count=(math $line - 1)
    commandline -f repaint
end
