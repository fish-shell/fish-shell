# system_profiler - report system hardware and software configuration (man 8 system_profiler)
#
# system_profiler is a flag-only tool (no single-dash long verbs).
# Data types are enumerated live via `system_profiler -listDataTypes`.

# ── live enumerators ─────────────────────────────────────────────────
function __fish_system_profiler_data_types
    system_profiler -listDataTypes 2>/dev/null | tail -n +2
end

# True when -detailLevel has already been given (so its argument should be offered).
function __fish_system_profiler_needs_detail_arg
    contains -- -detailLevel (commandline -opc)
end

# True when -timeout has already been given (so its argument should be offered).
function __fish_system_profiler_needs_timeout_arg
    contains -- -timeout (commandline -opc)
end

# ── flags ─────────────────────────────────────────────────────────────
complete -c system_profiler -f -o xml \
    -d 'Generate report in XML format (saveable as .spx for System Information.app)'
complete -c system_profiler -f -o json \
    -d 'Generate report in JSON format'
complete -c system_profiler -f -o listDataTypes \
    -d 'List available data types'
complete -c system_profiler -f -o detailLevel \
    -d 'Set level of detail for the report (mini, basic, full)'
complete -c system_profiler -f -o timeout \
    -d 'Maximum seconds to wait for results (0 = no timeout; default 180)'
complete -c system_profiler -f -o usage \
    -d 'Print usage info and examples'

# ── -detailLevel argument ─────────────────────────────────────────────
complete -c system_profiler -f -n __fish_system_profiler_needs_detail_arg \
    -a mini -d 'Report with no personal information'
complete -c system_profiler -f -n __fish_system_profiler_needs_detail_arg \
    -a basic -d 'Basic hardware and network information'
complete -c system_profiler -f -n __fish_system_profiler_needs_detail_arg \
    -a full -d 'All available information'

# ── data type positional arguments ────────────────────────────────────
complete -c system_profiler -f \
    -n 'not __fish_system_profiler_needs_detail_arg; and not __fish_system_profiler_needs_timeout_arg' \
    -a '(__fish_system_profiler_data_types)' \
    -d 'Data type to include in report'
