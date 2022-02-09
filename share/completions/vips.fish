# Regular completions
complete -c vips -s l -l list -d 'List objects'
complete -c vips -s p -l plugin -d 'Load PLUGIN'
complete -c vips -s v -l version -d 'Print version'
complete -c vips -l vips-concurrency -d 'Evaluate with N threads'
complete -c vips -l vips-progress -d 'Show progress feedback'
complete -c vips -l vips-leak -d 'Leak-check on exit'
complete -c vips -l vips-profile -d 'Profile and dump timing on exit'
complete -c vips -l vips-disc-threshold -d 'Decompress images larger than N'
complete -c vips -l vips-novector -d 'Disable vectorised operations'
complete -c vips -l vips-cache-max -d 'Cache at most N operations'
complete -c vips -l vips-cache-max-memory -d 'Cache at most N bytes in memory'
complete -c vips -l vips-cache-max-files -d 'Allow at most N open files'
complete -c vips -l vips-cache-trace -d 'Trace operation cache'
complete -c vips -l vips-cache-dump -d 'Dump operation cache on exit'
complete -c vips -l vips-version -d 'Print libvips version'
complete -c vips -l vips-config -d 'Print libvips config'
complete -c vips -l vips-pipe-read-limit -d 'Pipe read limit (bytes)'

# Operations
complete -c vips -n "__fish_is_nth_token 1" -xa "(__fish_vips_ops)"

function __fish_vips_ops
    vips -l | string match -rv _base | string replace -rf '^\s*\S+ \((.+?)\), +(\S.*?)(?:\s*[,(].*)?$' '$1\t$2'
end
