# minikube - is a tool that makes it easy to run Kubernetes
# locally. Minikube runs a single-node Kubernetes cluster inside a VM on
# your computer for users looking to try out Kubernetes or develop with it
# day-to-day.
# See: https://github.com/kubernetes/minikube

function __minikube_no_command
    set -l cmd (commandline -poc)
    if not set -q cmd[2]
        return 0
    end
    return 1
end

function __minikube_using_command
    set -l cmd (commandline -poc)

    if test (count $cmd) -gt (count $argv)
        set -e cmd[1]
        string match -q "$argv*" "$cmd"
        return $status
    end

    return 1
end

function __minikube_using_option
    set -l cmd (commandline -poc)
    set -l query "("(string join -- "|" (string escape --style=regex $argv))")"

    if test (count $cmd) -gt 1
        if string match -qr -- $query $cmd[-1]
            return 0
        end
    end
    return 1
end

function __minikube_using_option_value -a option -a value
    set -l cmd (commandline -poc)

    if test (count $cmd) -gt 1
        string match -qr -- (string escape --style=regex $option)"[= ]"(string escape --style=regex $value) "$cmd"
        return $status
    end

    return 1
end

function __minikube_list_subcommands
    echo addons\t"Modify kubernetes addons"
    echo completion\t"Output shell completion for SHELL"
    echo config\t"Modify config"
    echo dashboard\t"Open kubernetes dashboard URL"
    echo delete\t"Delete a local kubernetes cluster"
    echo docker-env\t"Set up docker env variables"
    echo get-k8s-versions\t"Get the list of available kubernetes versions"
    echo ip\t"Retrieve the IP address of the running cluster"
    echo logs\t"Get the logs of the running localkube instance"
    echo service\t"Get the kubernetes URL(s) for SERVICE"
    echo ssh\t"Log into or run a command on a machine with SSH"
    echo start\t"Start a local kubernetes cluster"
    echo status\t"Get the status of a local kubernetes cluster"
    echo stop\t"Stop a running local kubernetes cluster"
    echo version\t"Print the version"
end

function __minikube_list_addons
    if set -q argv[1]
        minikube addons list | string match -- "*$argv[1]*" | string replace -r -- "- ([^:]*): .*" '$1'
    else
        minikube addons list | string replace -r -- "- ([^:]*): .*" '$1'
    end
end

function __minikube_config_fields
    minikube config | string match ' \* *' | string replace ' * ' ''
end

# Sub-commands
complete -c minikube -f -n __minikube_no_command -a "(__minikube_list_subcommands)"

# Shared options
complete -c minikube -l alsologtostderr -d "Log to standard error as well as files"
complete -c minikube -l log_backtrace_at -d "When logging hits line file:N, emit a stack trace (default :0)"
complete -c minikube -l log_dir -d "Write log files in this directory"
complete -c minikube -l logtostderr -d "Log to standard error instead of files"
complete -c minikube -l show-libmachine-logs -d "Deprecated: To enable libmachine logs, set --v=3 or higher"
complete -c minikube -l stderrthreshold -d "Logs at or above this threshold go to stderr (default 2)"
complete -c minikube -l v -s v -d "Log level for logs"
complete -c minikube -l vmodule -d "Comma-separated list of pattern=N settings for file-filtered logging"

# Sub-command: addons
complete -c minikube -f -n "__minikube_using_command addons" -a disable -d "Disable the addon w/ADDON_NAME within minikube"
complete -c minikube -f -n "__minikube_using_command addons" -a enable -d "Enable the addon w/ADDON_NAME within minikube"
complete -c minikube -f -n "__minikube_using_command addons" -a list -d "List all available minikube addons as well as their current status"
complete -c minikube -f -n "__minikube_using_command addons" -a open -d "Open the addon w/ADDON_NAME within minikube"

complete -c minikube -n "__minikube_using_command addons" -l format -d "Go template format string for the addon list output"

complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from open" -l format -d "Format to output addons URL in (default http://{{.IP}}:{{.Port}}"
complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from open" -l https -d "Open the addons URL with https instead of http"
complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from open" -l url -d "Display the kubernetes addons URL instead of opening it"

complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from enable" -a "(__minikube_list_addons disabled)" -d Addon
complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from disable" -a "(__minikube_list_addons enabled)" -d Addon
complete -c minikube -n "__minikube_using_command addons; and __fish_seen_subcommand_from open" -a "(__minikube_list_addons)" -d Addon

# Sub-command: completion
complete -c minikube -f -n "__minikube_using_command completion" -a bash -d Shell

# Sub-command: config
complete -c minikube -f -n "__minikube_using_command config" -a get -d "Gets the value of PROPERTY_NAME from the minikube config file"
complete -c minikube -f -n "__minikube_using_command config" -a set -d "Sets an individual value in a minikube config file"
complete -c minikube -f -n "__minikube_using_command config" -a unset -d "Unsets an individual value in a minikube config file"
complete -c minikube -f -n "__minikube_using_command config" -a view -d "Display values currently set in the minikube config file"

complete -c minikube -n "__minikube_using_command config; and __fish_seen_subcommand_from get" -a "(__minikube_config_fields)" -d Property
complete -c minikube -n "__minikube_using_command config; and __fish_seen_subcommand_from set" -a "(__minikube_config_fields)" -d Property
complete -c minikube -n "__minikube_using_command config; and __fish_seen_subcommand_from unset" -a "(__minikube_config_fields)" -d Property
complete -c minikube -n "__minikube_using_command config; and __fish_seen_subcommand_from view" -a "(__minikube_config_fields)" -d Property
complete -c minikube -n "__minikube_using_command config; and __fish_seen_subcommand_from view" -l format -d "Go template format string for the config view output"

# Sub-command: dashboard
complete -c minikube -n "__minikube_using_command dashboard" -l url -d "Display the kubernetes dashboard URL instead of opening it"

# Sub-command: docker-env
complete -c minikube -n "__minikube_using_command docker-env" -l no-proxy -d "Add machine IP to NO_PROXY environment variable"
complete -c minikube -f -n "__minikube_using_command docker-env" -l shell -d "Force environment to be configured for a specified shell"
complete -c minikube -n "__minikube_using_command docker-env" -l unset -s u -d "Unset variables"

complete -c minikube -f -n "__minikube_using_command docker-env; and __fish_seen_subcommand_from --shell" -a "fish cmd powershell tcsh bash zsh" -d Shell

# Sub-command: service
complete -c minikube -f -n "__minikube_using_command config" -a list -d "List the URLs for the services in local cluster"

complete -c minikube -n "__minikube_using_command service" -l format -d "Format to output service URL in (default http://{{.IP}}:{{.Port}})"
complete -c minikube -n "__minikube_using_command service" -l https -d "Open the service URL with https instead of http"
complete -c minikube -n "__minikube_using_command service" -l namespace -s n -d "The service namespace (default default)"
complete -c minikube -n "__minikube_using_command service" -l url -d "Display the kubernetes service URL instead of opening it"

# Sub-command: start
complete -c minikube -n "__minikube_using_command start" -l container-runtime -d "The container runtime to be used"
complete -c minikube -n "__minikube_using_command start" -l cpus -d "Number of CPUs allocated to the minikube VM (default 2)"
complete -c minikube -n "__minikube_using_command start" -l disk-size -d "Disk size allocated to the minikube VM (format: <number>[<unit>]) (default 20g)"
complete -c minikube -n "__minikube_using_command start" -l docker-env -d "Environment variables to pass to the Docker daemon (format: key=value)"
complete -c minikube -n "__minikube_using_command start" -l extra-config -d "key=value pairs that describe config that may be passed to different components"
complete -c minikube -n "__minikube_using_command start" -l feature-gates -d "key=value pairs that describe feature gates for alpha/experimental features"
complete -c minikube -n "__minikube_using_command start" -l insecure-registry -d "Insecure Docker registries to pass to the Docker daemon"
complete -c minikube -n "__minikube_using_command start" -l iso-url -d "Location of the minikube iso"
complete -c minikube -n "__minikube_using_command start" -l keep-context -d "Keep the existing kubectl context and create a minikube context instead"
complete -c minikube -n "__minikube_using_command start" -l kubernetes-version -d "The kubernetes version that the minikube VM will use"
complete -c minikube -n "__minikube_using_command start" -l memory -d "Amount of RAM allocated to the minikube VM (default 2048)"
complete -c minikube -n "__minikube_using_command start" -l network-plugin -d "The name of the network plugin"
complete -c minikube -n "__minikube_using_command start" -l registry-mirror -d "Registry mirrors to pass to the Docker daemon"
complete -c minikube -f -n "__minikube_using_command start" -l vm-driver -d "VM driver to use (default virtualbox)"

complete -c minikube -f -n "__minikube_using_command start; and __minikube_using_option --vm-driver" -a "virtualbox kvm hyperv" -d "VM driver"

complete -c minikube -n "__minikube_using_command start; and __minikube_using_option_value --vm-driver virtualbox" -l host-only-cidr -d "The CIDR to be used for the VM"
complete -c minikube -n "__minikube_using_command start; and __minikube_using_option_value --vm-driver kvm" -l kvm-network -d "The KVM network name"
complete -c minikube -n "__minikube_using_command start; and __minikube_using_option_value --vm-driver hyperv" -l hyperv-virtual-switch -d "The hyperv virtual switch name"

# Sub-command: status
complete -c minikube -n "__minikube_using_command status" -l format -d "Go template format string for the status output"
