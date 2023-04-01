# Explicitly overriding HOME/XDG_CONFIG_HOME is only required if not invoking via `make test`
# RUN: %ghoti -C 'set -g ghoti %ghoti' %s

mkdir -p $XDG_CONFIG_HOME/ghoti

# ghoti_variables
set -l target_file $XDG_CONFIG_HOME/ghoti/target_ghoti_variables
set -l ghoti_variables $XDG_CONFIG_HOME/ghoti/ghoti_variables
set -l backup_file $XDG_CONFIG_HOME/ghoti/ghoti_variables_backup

echo >$target_file
cp $target_file $backup_file
ln -sf $target_file $ghoti_variables
$ghoti -c 'set -U variable value'

if not test -L $ghoti_variables
    echo ghoti_variables has been overwritten
else if cmp $target_file $backup_file >/dev/null
    echo ghoti_variables was never written
else
    echo ghoti_variables is still a symlink
end
# CHECK: ghoti_variables is still a symlink


# ghoti_history
set -l history_file $XDG_DATA_HOME/ghoti/ghoti_history
set -l target_file $XDG_DATA_HOME/ghoti/target_ghoti_history
set -l backup_file $XDG_DATA_HOME/ghoti/ghoti_history_backup

echo '- cmd: echo I will be deleted from history
        when: 1614577746' >$target_file
cp $target_file $backup_file
ln -sf $target_file $history_file
# The one way to ensure non-interactive ghoti writes to the history file
$ghoti -c 'echo all | history delete deleted | grep echo'
# CHECK: [1] echo I will be deleted from history

if not test -L $history_file
    echo ghoti_history has been overwritten
else if cmp $target_file $backup_file &>/dev/null
    # cmp writes to stderr when one file is empty, thus &> above
    echo ghoti_history was never written
else
    echo ghoti_history is still a symlink
end
# CHECK: ghoti_history is still a symlink
