function fish_clipboard_copy
	if type -q pbcopy
        commandline | pbcopy
    else if type -q xsel
        commandline | xsel --clipboard
    end
end
