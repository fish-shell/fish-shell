# SYNOPSIS
#        kmutil <subcommand>
#        kmutil <load|unload|showloaded>
#        kmutil <find|libraries|print-diagnostics>
#        kmutil <create|inspect|check|log|dumpstate>
#        kmutil <clear-staging|trigger-panic-medic>
#        kmutil -h

if test "$(command -s kmutil)" = /usr/bin/kmutil
    __fish_cache_sourced_completions kmutil --generate-completion-script=fish
    or command kmutil --generate-completion-script=fish | source
end
