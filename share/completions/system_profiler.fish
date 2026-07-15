function __fish_system_profiler_data_types
    system_profiler -listDataTypes | tail -n +2
end

complete -d 'Generate report in XML format (saveable as .spx for System Information.app)' -c system_profiler -o xml
complete -c system_profiler -o json -d 'Generate report in JSON format'
complete -c system_profiler -o listDataTypes -d 'List available data types'
complete -c system_profiler -d 'Set level of detail for the report' -f -o detailLevel -x -a 'mini basic full'
complete -c system_profiler -d 'Maximum seconds to wait for results (0 = no timeout; default 180)' -f -o timeout -x
complete -c system_profiler -o usage -d 'Print usage info and examples'

complete -c system_profiler -d 'Data type to include in report' -f -a '(__fish_system_profiler_data_types)'
