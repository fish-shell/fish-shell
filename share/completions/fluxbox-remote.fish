
# magic completion safety check (do not remove this comment)
if not type -q fluxbox-remote
    exit
end
complete -c fluxbox-remote -xa '(fluxbox -list-commands)'
