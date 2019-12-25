if command -sq pipenv
    # pipenv determines which completions to print via $SHELL, so override it for this.
    SHELL=(status fish-path) pipenv --completion 2>/dev/null | source
end
