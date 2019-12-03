function edit_command_buffer --description 'Edit the command buffer in an external editor'
    set -l f (mktemp)
    if set -q f[1]
        mv $f $f.fish
        set f $f.fish
    else
        # We should never execute this block but better to be paranoid.
        if set -q TMPDIR
            set f $TMPDIR/fish.$fish_pid.fish
        else
            set f /tmp/fish.$fish_pid.fish
        end
        touch $f
        or return 1
    end

    # Edit the command line with the users preferred editor or vim or emacs.
    set -l editor
    if set -q VISUAL
        echo $VISUAL | read -at editor
    else if set -q EDITOR
        echo $EDITOR | read -at editor
    else
        echo
        echo (_ 'External editor requested but $VISUAL or $EDITOR not set.')
        echo (_ 'Please set VISUAL or EDITOR to your preferred editor.')
        commandline -f repaint
        return 1
    end

    commandline -b >$f
    __fish_disable_bracketed_paste
    $editor $f
    set -l editor_status $status
    __fish_enable_bracketed_paste

    # Here we're checking the exit status of the editor.
    if test $editor_status -eq 0 -a -s $f
        # Set the command to the output of the edited command and move the cursor to the
        # end of the edited command.
        commandline -r -- (cat $f)
        commandline -C 999999
    else
        echo
        echo (_ "Ignoring the output of your editor since its exit status was non-zero")
        echo (_ "or the file was empty")
    end
    command rm $f
    # We've probably opened something that messed with the screen.
    # A repaint seems in order.
    commandline -f repaint
end
