# plain vi (as bundled with SunOS 5.8)
# todo:
# -wn : Set the default window size to n
# +command : same as -c command

# Check if vi is really vim
if vi --version >/dev/null 2>/dev/null
    complete -c vi -w vim
else
    complete -c vi -s s -d 'Suppress all interactive user feedback'
    complete -c vi -s C -d 'Encrypt/decrypt text'
    complete -c vi -s l -d 'Set up for editing LISP programs'
    complete -c vi -s L -d 'List saved file names after crash'
    complete -c vi -s R -d 'Read-only mode'
    complete -c vi -s S -d 'Use linear search for tags if tag file not sorted'
    complete -c vi -s v -d 'Start in display editing state'
    complete -c vi -s V -d 'Verbose mode'
    complete -c vi -s x -d 'Encrypt/decrypt text'
    complete -c vi -r -s r -d 'Recover file after crash'
    complete -c vi -r -s t -d 'Edit the file containing a tag'
    complete -c vi -r -c t -d 'Begin editing by executing the specified  editor command'
end
