# Completions for the eix tool's update-eix-remote command
# http://dev.croup.de/proj/eix and http://sourceforge.net/projects/eix/

# magic completion safety check (do not remove this comment)
if not type -q update-eix-remote
    exit
end

# Author: Tassilo Horn <tassilo@member.fsf.org>

##########
# UPDATE-EIX-REMOTE
#####
# Options
complete -c update-eix-remote -s q -d "Be quiet"
complete -c update-eix-remote -s v -d "Be verbose (default)"
complete -c update-eix-remote -s u -xua '(__fish_print_users)' -d "Call wget as the given USER"
complete -c update-eix-remote -s o -d "Use the given PATH as $OVERLAYPARENT"
#####
# Subcommands
complete -c update-eix-remote -xa 'update\t"'(_ "Fetch the eix-caches of some layman overlays into a temporary file resp. into FILE and add them to the eix database")'" fetch\t"'(_ "Only fetch the overlays into FILE")'" add\t"'(_ "Only add the overlays from FILE to the eix database")'" remove\t"'(_ "Remove all temporarily added virtual overlays from the eix database")'"'
##########
