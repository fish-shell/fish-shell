if command -sq pip3
    pip3 completion --fish | sed 's/-c pip/-c pip3/' | source
end
