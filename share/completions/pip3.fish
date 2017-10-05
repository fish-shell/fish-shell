if command -sq pip3
    pip3 completion --fish | string replace -r "\b-c\s+pip\b" -- "-c pip3" | source
end
