# Completions for the eix tool's eix-sync command
# http://dev.croup.de/proj/eix and http://sourceforge.net/projects/eix/

# Author: Tassilo Horn <tassilo@member.fsf.org>

##########
# EIX-SYNC
complete -c eix-sync -s i -d "Ignore all previous options"
complete -c eix-sync -s d -d "Only show differences to the previously saved database and exit"
complete -c eix-sync -s s -xa '(__fish_print_users)@(__fish_print_hostnames):' -d "Sync via rsync from the given SERVER"
complete -c eix-sync -s c -xa '(__fish_print_users)@(__fish_print_hostnames):' -d "Sync via rsync *to* the given CLIENT"
complete -c eix-sync -s U -d "Do not touch the database and omit the hooks after update-eix. (Implies -R)"
complete -c eix-sync -s u -d "Update database only and show differences"
complete -c eix-sync -s g -d "Do not call gensync (and the !commands in /etc/eix-sync.conf)"
complete -c eix-sync -s @ -d "Do not execute the hooks of /etc/eix-sync.conf"
complete -c eix-sync -s S -d "Do not execute the hooks after emerge --sync (@@ entries)"
complete -c eix-sync -s m -d "Run emerge --metadata instead of emerge --sync"
complete -c eix-sync -s t -d "Use temporary file to save the current database"
complete -c eix-sync -s v -d "Don't suppress output of update-eix and emerge"
complete -c eix-sync -s q -d "Be quiet (close stdout)"
complete -c eix-sync -s w -d "Run emerge-webrsync instead of emerge --sync"
complete -c eix-sync -s r -d "Really recreate the dep-cache (rm -rf /var/cache/edb/dep/*) (default)"
complete -c eix-sync -s R -d "Do not really recreate the dep-cache"
complete -c eix-sync -s h -d "Show a short help text and exit"
##########
