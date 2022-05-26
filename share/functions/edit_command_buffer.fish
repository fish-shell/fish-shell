function edit_command_buffer --description 'Edit the command buffer in an external editor'
    set -l f (mktemp)
    or return 1
    if set -q f[1]
        command mv $f $f.fish
        set f $f.fish
    else
        # We should never execute this block but better to be paranoid.
        if set -q TMPDIR
            set f $TMPDIR/fish.$fish_pid.fish
        else
            set f /tmp/fish.$fish_pid.fish
        end
        command touch $f
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
    set -l offset (commandline --cursor)
    # compute cursor line/column
    set -l lines (commandline)\n
    set -l line 1
    while test $offset -ge (string length -- $lines[1])
        set offset (math $offset - (string length -- $lines[1]))
        set line (math $line + 1)
        set -e lines[1]
    end
    set col (math $offset + 1)

    set -l basename (string match -r '[^/]*$' -- $editor[1])
    switch $basename
        case vi vim nvim
            set -a editor +$line +"norm! $col|" $f
        case emacs emacsclient gedit kak
            set -a editor +$line:$col $f
        case nano
            set -a editor +$line,$col $f
        case joe ee
            set -a editor +$line $f
        case code code-oss
            set -a editor --goto $f:$line:$col --wait
        case subl
            set -a editor $f:$line:$col --wait
        case micro
            set -a editor $f +$line:$col
        case '*'
            set -a editor $f
    end

    __fish_disable_bracketed_paste
    $editor
    set -l editor_status $status
    __fish_enable_bracketed_paste

    # Here we're checking the exit status of the editor.
    if test $editor_status -eq 0 -a -s $f
        # Set the command to the output of the edited command and move the cursor to the
        # end of the edited command.
        commandline -r -- (command cat $f)
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
