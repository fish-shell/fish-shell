
# magic completion safety check (do not remove this comment)
if not type -q cabal-dev
    exit
end
complete -c cabal-dev -w cabal
