# plain vi (as bundled with SunOS 5.8)
# todo:
# -wn : Set the default window size to n
# +command : same as -c command


# Check if vi is really vim
if vi --version > /dev/null ^ /dev/null
    complete -c vi -w vim
else
    complete -c vi -s s --description 'Suppress all interactive user feedback'
    complete -c vi -s C --description 'Encrypt/decrypt text'
    complete -c vi -s l --description 'Set up for editing LISP programs'
    complete -c vi -s L --description 'List saved file names after crash'
    complete -c vi -s R --description 'Read-only mode'
    complete -c vi -s S --description 'Use linear search for tags if tag file not sorted'
    complete -c vi -s v --description 'Start in display editing state'
    complete -c vi -s V --description 'Verbose mode'
    complete -c vi -s x --description 'Encrypt/decrypt text'
    complete -c vi -r -s r --description 'Recover file after crash'
    complete -c vi -r -s t --description 'Edit the file containing a tag'
    complete -c vi -r -c t --description 'Begin editing by executing the specified  editor command'
end

