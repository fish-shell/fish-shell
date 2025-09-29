# localization: tier1
function prompt_hostname --description 'short hostname for the prompt'
    string replace -r -- "\..*" "" $hostname
end
