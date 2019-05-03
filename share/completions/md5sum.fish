complete -c md5sum -d "Compute and check message digest" -r
complete -c md5sum -s b -l binary -d 'Read in binary mode'
complete -c md5sum -s c -l check -d "Read sums from files and check them"
complete -c md5sum -s t -l text -d 'Read in text mode'
complete -c md5sum -l quiet -d 'Don''t print OK for each successfully verified file'
complete -c md5sum -l status -d 'Don''t output anything, status code shows success'
complete -c md5sum -s w -l warn -d 'Warn about improperly formatted checksum lines'
complete -c md5sum -l strict -d 'With --check, exit non-zero for any invalid input'
complete -c md5sum -l help -d 'Display help text'
complete -c md5sum -l version -d 'Output version information and exit'
