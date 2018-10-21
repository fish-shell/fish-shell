function prompt_hostname
    # return the short hostname only by default (#4804)
    string replace -r "\..*" "" $hostname
end
