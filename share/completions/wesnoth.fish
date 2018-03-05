# Command specific completions for the wesnoth command from The Battle For Wesnoth (a strategy game)
# http://www.wesnoth.org/

# magic completion safety check (do not remove this comment)
if not type -q wesnoth
    exit
end

complete -c wesnoth -l bpp -d 'Number sets BitsPerPixel value'
complete -c wesnoth -l compress -d '<infile> <outfile> compresses a savefile (infile) that is in text WML format into binary WML format (outfile)'
complete -c wesnoth -s d -l debug -d 'Shows extra debugging information and enables additional command mode options in-game'
complete -c wesnoth -l decompress -d '<infile> <outfile> decompresses a savefile (infile) that is in binary WML format into text WML format (outfile)'
complete -c wesnoth -s f -l fullscreen -d 'Runs the game in full screen mode'
complete -c wesnoth -l fps -d 'Displays the number of frames per second the game is currently running at, in a corner of the screen'
complete -c wesnoth -s h -l help -d 'Displays a summary of command line options to standard output, and exits'
complete -c wesnoth -l load -d 'Savegame loads the file savegame from the standard save game directory'
complete -c wesnoth -l log-error -l log-warning -l log-info -d 'Set the severity level of debugging domains'
complete -c wesnoth -l multiplayer -d 'Runs a multiplayer game'
complete -c wesnoth -l nocache -d 'Disables caching of game data'
complete -c wesnoth -l nosound -d 'Runs the game without sounds and music'
complete -c wesnoth -l path -d 'Prints the name of the game data directory and exits'
complete -c wesnoth -s r -l resolution -d 'XxY sets the screen resolution'
complete -c wesnoth -s t -l test -d 'Runs the game in a small test scenario'
complete -c wesnoth -s v -l version -d 'Shows the version number and exits'
complete -c wesnoth -s w -l windowed -d 'Runs the game in windowed mode'
complete -c wesnoth -l no-delay -d 'Runs the game without any delays for graphic benchmarking'
complete -c wesnoth -l exit-at-end -d 'Exits once the scenario is over, without displaying victory/defeat dialog which requires the user to click OK'
complete -c wesnoth -l algorithm -d 'Selects a non-standard algorithm to be used by the AI controller for this side'
complete -c wesnoth -l controller -d 'Selects the controller for this side'
complete -c wesnoth -l era -d 'Use this option to play in the selected era instead of the "Default" era'
complete -c wesnoth -l nogui -d 'Runs the game without the GUI'
complete -c wesnoth -l parm -d 'Sets additional parameters for this side'
complete -c wesnoth -l scenario -d 'Selects a multiplayer scenario'
complete -c wesnoth -l side -d 'Selects a faction of the current era for this side'
complete -c wesnoth -l turns -d 'Sets the number of turns for the chosen scenario'
