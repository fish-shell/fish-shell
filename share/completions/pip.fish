
# magic completion safety check (do not remove this comment)
if not type -q pip
    exit
end
if command -sq pip
    pip completion --fish | source
end

