function __fish_preview_current_file --description "Open the file at the cursor in a pager"
    if __fish_edit_command_if_at_cursor
        return 0
    end

    set -l pager (__fish_anypager)
    or set pager cat

    # commandline -t will never return an empty list. However, the token
    # could comprise multiple lines, so join them into a single string.
    set -l file (commandline -t | string collect)
    set -l prefix eval set

    if test -z $file
        # $backslash will parsed as regex which may need additional escaping.
        set -l backslash '\\\\'
        not status test-feature regex-easyesc && set backslash $backslash$backslash
        set file (string replace -ra -- '([ ;#^<>&|()"\'])' "$backslash\$1" (commandline -xc)[-1])
        set prefix set
    end

    set -q file[1] || return

    # strip -option= from token if present
    set file (string replace -r -- '^-[^\s=]*=' '' $file | string collect)

    $prefix -l files $file || return # Bail if $file does not tokenize.

    if set -q files[1] && test -f $files[1]
        $pager $files
        commandline -f repaint
    end
end
