function __fish_print_abook_emails --description 'Print email addresses (abook)'
    abook --mutt-query "" | string match -r -v '^\s*$'

end

if command -sq abook
    complete -c mutt -f -a '(__fish_print_abook_emails)'
    complete -c mutt -s c -x -d 'Specify a carbon-copy (CC) recipient' -a '(__fish_print_abook_emails)'
    complete -c mutt -s b -x -d 'Specify a blind-carbon-copy (BCC) recipient' -a '(__fish_print_abook_emails)'
end

complete -c mutt -s D -d 'Print the value of all configuration options to stdout'
complete -c mutt -s h -d 'Display help'
complete -c mutt -s n -d 'Bypass the system configuration file'
complete -c mutt -s p -d 'Resume a postponed message'
complete -c mutt -s R -d 'Open a mailbox in read-only mode'
complete -c mutt -s v -d 'Display the Mutt version number and compile-time definitions'
complete -c mutt -s x -d 'Emulate the mailx compose mode'
complete -c mutt -s y -d 'Start Mutt with a listing of all mailboxes'
complete -c mutt -s z -d 'When used with -f, causes Mutt not to start if there are no messages'
complete -c mutt -s Z -d 'Open the first mailbox which contains new mail'

complete -r -c mutt -s A -d 'An expanded version of the given alias is passed to stdout'
complete -r -c mutt -s a -d 'Attach a file to your message using MIME'
complete -r -c mutt -s b -d 'Specify a blind-carbon-copy (BCC) recipient'
complete -r -c mutt -s c -d 'Specify a carbon-copy (CC) recipient'
complete -r -c mutt -s e -d 'Run command after processing of initialization files'
complete -r -c mutt -s f -d 'Specify which mailbox to load'
complete -r -c mutt -s F -d 'Specify an initialization file to read instead of ~/.muttrc'
complete -r -c mutt -s H -d 'Specify a draft file containing header and body for the message'
complete -r -c mutt -s i -d 'Specify a file to include into the body of a message'
complete -r -c mutt -s m -d 'Specify a default mailbox type'
complete -r -c mutt -s Q -d 'Query a configuration variables value'
complete -r -c mutt -s s -d 'Specify the subject of the message'
