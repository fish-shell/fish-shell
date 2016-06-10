function fish_clipboard_paste
	if type -q pbpaste
        commandline -i (pbpaste)
    else if type -q xsel
        commandline -i (xsel --clipboard)
    end
end
