#RUN: %fish -C 'set -g fish %fish' %s
begin
    set -gx XDG_CONFIG_HOME (mktemp -d)
    mkdir -p $XDG_CONFIG_HOME/fish
    set -l target_file $XDG_CONFIG_HOME/fish/target_fish_variables
    set -l fish_variables $XDG_CONFIG_HOME/fish/fish_variables

    echo > $target_file
    ln -sf $target_file $fish_variables
    $fish -c 'set -U variable value'

    if test -L $fish_variables
       echo fish_variables is still a symlink
    else
       echo fish_variables has been overwritten
    end
    # CHECK: fish_variables is still a symlink
end
