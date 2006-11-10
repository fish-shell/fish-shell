# Completions for the eix tool's update-eix command
# http://dev.croup.de/proj/eix and http://sourceforge.net/projects/eix/

# Author: Tassilo Horn <tassilo@member.fsf.org>

##########
# UPDATE-EIX
complete -c update-eix -s h -l help -d "Show a short help screen"
complete -c update-eix -s V -l version -d "Show version string"
complete -c update-eix -l dump -d "Show eixrc-variables"
complete -c update-eix -l dump-defaults -d "Show default eixrc-variables"
complete -c update-eix -s q -l quiet -d "Produce no output"
complete -c update-eix -s o -l output -d "Output to the given file"
complete -c update-eix -s x -l exclude-overlay -d "Exclude the given overlay from the update-process"
complete -c update-eix -s a -l add-overlay -d "Add the given overlay to the update-process"
complete -c update-eix -s m -l override-method -d "Override cache method for a specified overlay"
##########
