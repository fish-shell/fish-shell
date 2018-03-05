#
# Command specific completions for the awk command.
# These completions where generated from the commands
# man page by the make_completions.py script, but may
# have been hand edited since.
#

# magic completion safety check (do not remove this comment)
if not type -q awk
    exit
end

complete -c awk -s F -d 'Define the input field separator to be the extended regular expression ERE, before any input is read; see Regular Expressions' -r
complete -c awk -s f -d 'Specify the pathname of the file progfile containing an awk program' -r
complete -c awk -s v -d 'The application shall ensure that the assignment argument is in the same form as an assignment operand' -r
