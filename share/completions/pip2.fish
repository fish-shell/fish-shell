if command -sq pip2
    pip2 completion --fish | sed 's/-c pip/-c pip2/' | source
end
