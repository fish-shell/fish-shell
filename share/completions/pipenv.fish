if command -sq pipenv
    # pipenv determines which completions to print via some automagic that might not be perfect.
    # Force it to be correct.
    _PIPENV_COMPLETE=source-fish pipenv --completion 2>/dev/null | source
end
