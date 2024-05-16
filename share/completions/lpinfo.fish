__fish_complete_lpr lpinfo
complete -c lpinfo -s l -d 'Shows a "long" listing of devices or drivers'
complete -c lpinfo -l device-id -x -d 'Specifies the IEEE-1284 device ID to match when listing drivers with the -m option'
complete -c lpinfo -l exclude-schemes -x -d 'comma-separated list of device or PPD schemes that should be excluded from the results'
complete -c lpinfo -l include-schemes -x -d 'comma-separated list of device or PPD schemes that should be included in the results'
complete -c lpinfo -l language -x -d 'Specifies the language to match when listing drivers with the -m option'
complete -c lpinfo -l make-and-model -x -d 'Specifies the make and model to match when listing drivers with the -m option'
complete -c lpinfo -l product -x -d 'Specifies the product to match when listing drivers with the -m option'
complete -c lpinfo -l timeout -x -d 'Specifies the timeout when listing devices with the -v option (sec)'
