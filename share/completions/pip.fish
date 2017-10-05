if command -sq pip
    eval (pip completion --fish)
end

# Some distributions prefer to have separate pip commands
# depending on the version of Python running
if command -sq pip2
    eval (pip2 completion --fish)
end

if command -sq pip3
    eval (pip3 completion --fish)
end
