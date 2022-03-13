complete -c gendarme -l version -d 'Show version'

complete -c gendarme -l config -r -d 'Use config file'
complete -c gendarme -l set -x -d 'Use rule set from config file'
complete -c gendarme -l xml -r -d 'Convert stdout to XML and redirect to file'
complete -c gendarme -l html -r -d 'Convert stdout to HTML and redirect to file'
complete -c gendarme -l console -d 'Show the defects on stdout'
complete -c gendarme -l ignore -r -d 'Exclude the defects from file'
complete -c gendarme -l limit -x -d 'Specify defect limit'
complete -c gendarme -l severity -x \
    -a 'all audit audit+ low low+ low- medium medium+ medium- high high+ high- critical critical-' \
    -d 'Filter the reported defects to include the specified severity levels'
complete -c gendarme -l confidence -x \
    -a 'all low low+ normal normal+ normal- high high+ high- total total-' \
    -d 'Filter the reported defects to include the specified confidence levels'
complete -c gendarme -l quiet -d 'Discard stdout'
complete -c gendarme -s v -l verbose -d 'Show more messages'
