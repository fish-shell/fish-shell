# plain vi (as bundled with SunOS 5.8)
# todo:
# -wn : Set the default window size to n
# +command : same as -c command


# Check if vi is really vim
if vi --version > /dev/null ^ /dev/null
    complete -c vi -w vim
else
    complete vi -s s --description 'Suppress all interactive user feedback'
    complete vi -s C --description 'Encrypt/decrypt text'
    complete vi -s l --description 'Set up for editing LISP programs'
    complete vi -s L --description 'List saved file names after crash'
    complete vi -s R --description 'Read-only mode'
    complete vi -s S --description 'Use linear search for tags if tag file not sorted'
    complete vi -s v --description 'Start in display editing state'
    complete vi -s V --description 'Verbose mode'
    complete vi -s x --description 'Encrypt/decrypt text'

    complete vi -r -s r --description 'Recover file after crash'
    complete vi -r -s t --description 'Edit the file containing a tag'
    complete vi -r -c t --description 'Begin editing by executing the specified  editor command'
end

