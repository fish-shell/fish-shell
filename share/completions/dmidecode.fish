function __fish_dmidecode_complete_keywords
    dmidecode -s 2>&1 1>/dev/null | string match -rg '^\s+(.*)'
end

function __fish_dmidecode_complete_types
    dmidecode -t 2>&1 1>/dev/null | string match -rg '^\s+(.*)'
end

complete -c dmidecode -f
complete -c dmidecode -s d -l dev-mem -r -d 'Memory device file'
complete -c dmidecode -s q -l quiet -d 'Be less verbose'
complete -c dmidecode -l no-quirks -d 'Decode everything exactly as it is'
complete -c dmidecode -s s -l string -xa '(__fish_dmidecode_complete_keywords)' -d 'Only display specified value'
complete -c dmidecode -s t -l type -xa '(__fish_dmidecode_complete_types)' -d 'Only display entries of specified type'
complete -c dmidecode -s H -l handle -x -d 'Only display specified handle'
complete -c dmidecode -s u -l dump -d 'Do not decode the entries'
complete -c dmidecode -l dump-bin -r -d 'Dump DMI data to a file'
complete -c dmidecode -l from-dump -r -d 'Read DMI data generated using --dump-bin'
complete -c dmidecode -l no-sysfs -d 'Do not attempt to read DMI data from sysfs files'
complete -c dmidecode -l oem-string -x -d 'Only display the value of the OEM string number N'
complete -c dmidecode -s h -l help -d 'Display usage information and exit'
complete -c dmidecode -s V -l version -d 'Display the version and exit'
