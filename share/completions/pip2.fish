if command -sq pip2
    pip2 completion --fish | string replace -r "\b-c\s+pip\b" -- "-c pip2" | source
end
