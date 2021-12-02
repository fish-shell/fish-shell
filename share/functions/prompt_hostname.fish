function prompt_hostname --description 'Print the hostname, shortened for use in the prompt'
    string replace -r "\..*" "" $hostname
end
