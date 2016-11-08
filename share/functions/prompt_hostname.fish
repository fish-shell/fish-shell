# Fetching the host name can be expensive if there is a problem with DNS or whatever subsystem the
# hostname command uses. So cache the answer so including it in the prompt doesn't make fish seem
# slow.
if not set -q __fish_prompt_hostname
    set -g __fish_prompt_hostname (hostname | string split '.')[1]
end

function prompt_hostname
    echo $__fish_prompt_hostname
end
