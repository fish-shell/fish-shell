
# magic completion safety check (do not remove this comment)
if not type -q pipenv
    exit
end
if command -sq pipenv
    pipenv --completion | source
end
