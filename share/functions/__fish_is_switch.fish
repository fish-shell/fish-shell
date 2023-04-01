# Whether or not the current token is a switch
function __ghoti_is_switch
    string match -qr -- '^-' ""(commandline -ct)
end
