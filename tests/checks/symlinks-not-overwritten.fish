# RUN: fish=%fish %fish %s

# fish_variables
set -l target_file $XDG_CONFIG_HOME/fish/target_fish_variables
set -l fish_variables $XDG_CONFIG_HOME/fish/fish_variables
set -l backup_file $XDG_CONFIG_HOME/fish/fish_variables_backup

echo >$target_file
cp $target_file $backup_file
ln -sf $target_file $fish_variables
$fish -c 'set -U variable value'

if not test -L $fish_variables
    echo fish_variables has been overwritten
else if cmp $target_file $backup_file >/dev/null
    echo fish_variables was never written
else
    echo fish_variables is still a symlink
end
# CHECK: fish_variables is still a symlink
rm $fish_variables


# fish_history
set -l history_file $XDG_DATA_HOME/fish/fish_history
set -l target_file $XDG_DATA_HOME/fish/target_fish_history
set -l backup_file $XDG_DATA_HOME/fish/fish_history_backup

echo '- cmd: echo I will be deleted from history
        when: 1614577746' >$target_file
cp $target_file $backup_file
ln -sf $target_file $history_file
# The one way to ensure non-interactive fish writes to the history file
$fish -c 'echo all | history delete deleted | grep echo'
# CHECK: [1] echo I will be deleted from history

if not test -L $history_file
    echo fish_history has been overwritten
else if cmp $target_file $backup_file &>/dev/null
    # cmp writes to stderr when one file is empty, thus &> above
    echo fish_history was never written
else
    echo fish_history is still a symlink
end
# CHECK: fish_history is still a symlink
rm $history_file
