# TODO: This function is deprecated. It was used in fish_clipboard_paste
# which some users copied, so maybe leave it around for a few years.
function __fish_commandline_is_singlequoted --description "Return 0 if the current token has an open single-quote"
    string match -q 'single*' (__fish_tokenizer_state -- (commandline -ct | string collect))
end
