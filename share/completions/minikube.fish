function __minikube_no_command
    set -l cmd (commandline -poc)
    set -e cmd[1]
    if test (count $cmd) -eq 0
        return 0
    end
    return 1
end

function __minikube_using_command
    set cmd (commandline -poc)
    if test (count $cmd) -gt 1
        if test $argv[1] = $cmd[2]
            return 0
        end
    end
    return 1
end

complete -c minikube -f -n "__minikube_no_command" -a addons -d "Modify minikube's kubernetes addons"
complete -c minikube -f -n "__minikube_no_command" -a completion -d "Outputs minikube shell completion for the given shell (bash)"
complete -c minikube -f -n "__minikube_no_command" -a config -d "Modify minikube config"
complete -c minikube -f -n "__minikube_no_command" -a dashboard -d "Opens/displays the kubernetes dashboard URL for your local cluster"
complete -c minikube -f -n "__minikube_no_command" -a delete -d "Deletes a local kubernetes cluster."
complete -c minikube -f -n "__minikube_no_command" -a docker-env -d "sets up docker env variables; similar to '\$(docker-machine env)'"
complete -c minikube -f -n "__minikube_no_command" -a get-k8s-versions -d "Gets the list of available kubernetes versions available for minikube."
complete -c minikube -f -n "__minikube_no_command" -a ip -d "Retrieve the IP address of the running cluster."
complete -c minikube -f -n "__minikube_no_command" -a logs -d "Gets the logs of the running localkube instance, used for debugging minikube, not user code."
complete -c minikube -f -n "__minikube_no_command" -a service -d "Gets the kubernetes URL(s) for the specified service in your local cluster"
complete -c minikube -f -n "__minikube_no_command" -a ssh -d "Log into or run a command on a machine with SSH; similar to 'docker-machine ssh'"
complete -c minikube -f -n "__minikube_no_command" -a start -d "Starts a local kubernetes cluster."
complete -c minikube -f -n "__minikube_no_command" -a status -d "Gets the status of a local kubernetes cluster."
complete -c minikube -f -n "__minikube_no_command" -a stop -d "Stops a running local kubernetes cluster."
complete -c minikube -f -n "__minikube_no_command" -a version -d "Print the version of minikube."
complete -c minikube -l "alsologtostderr" -d "log to standard error as well as files"
complete -c minikube -l "log_backtrace_at" -d "when logging hits line file:N, emit a stack trace (default :0)"
complete -c minikube -l "log_dir" -d "If non-empty, write log files in this directory (default \"\")"
complete -c minikube -l "logtostderr" -d "log to standard error instead of files"
complete -c minikube -l "show-libmachine-logs" -d "Deprecated: To enable libmachine logs, set --v=3 or higher"
complete -c minikube -l "stderrthreshold" -d "logs at or above this threshold go to stderr (default 2)"
complete -c minikube -s "v" -d "log level for V logs"
complete -c minikube -l "v" -d "log level for V logs"
complete -c minikube -l "vmodule" -d "comma-separated list of pattern=N settings for file-filtered logging"
complete -c minikube -n "__minikube_using_command addons" -l "format" -d "Go template format string for the addon list output.  The format for Go templates can be found here: https://golang.org/pkg/text/template/"
complete -c minikube -n "__minikube_using_command addons" -l "format" -d "Format to output addons URL in.  This format will be applied to each url individually and they will be printed one at a time. (default \"http://{{.IP}}:{{.Port}}\")"
complete -c minikube -n "__minikube_using_command addons" -l "https" -d "Open the addons URL with https instead of http"
complete -c minikube -n "__minikube_using_command addons" -l "url" -d "Display the kubernetes addons URL in the CLI instead of opening it in the default browser"
complete -c minikube -n "__minikube_using_command config" -l "format" -d "Go template format string for the config view output.  The format for Go templates can be found here: https://golang.org/pkg/text/template/"
complete -c minikube -n "__minikube_using_command dashboard" -l "url" -d "Display the kubernetes dashboard in the CLI instead of opening it in the default browser"
complete -c minikube -n "__minikube_using_command docker-env" -l "no-proxy" -d "Add machine IP to NO_PROXY environment variable"
complete -c minikube -n "__minikube_using_command docker-env" -l "shell" -d "Force environment to be configured for a specified shell: [fish, cmd, powershell, tcsh, bash, zsh], default is auto-detect"
complete -c minikube -n "__minikube_using_command docker-env" -s "u" -d "Unset variables instead of setting them"
complete -c minikube -n "__minikube_using_command docker-env" -l "unset" -d "Unset variables instead of setting them"
complete -c minikube -n "__minikube_using_command service" -l "format" -d "Format to output service URL in. This format will be applied to each url individually and they will be printed one at a time. (default \"http://{{.IP}}:{{.Port}}\")"
complete -c minikube -n "__minikube_using_command service" -l "https" -d "Open the service URL with https instead of http"
complete -c minikube -n "__minikube_using_command service" -s "n" -d "The service namespace (default \"default\")"
complete -c minikube -n "__minikube_using_command service" -l "namespace" -d "The service namespace (default \"default\")"
complete -c minikube -n "__minikube_using_command service" -l "url" -d "Display the kubernetes service URL in the CLI instead of opening it in the default browser"
complete -c minikube -n "__minikube_using_command service" -s "n" -d "The services namespace"
complete -c minikube -n "__minikube_using_command service" -l "namespace" -d "The services namespace"
complete -c minikube -n "__minikube_using_command service" -l "format" -d "Format to output service URL in. This format will be applied to each url individually and they will be printed one at a time. (default \"http://{{.IP}}:{{.Port}}\")"
complete -c minikube -n "__minikube_using_command start" -l "container-runtime" -d "The container runtime to be used"
complete -c minikube -n "__minikube_using_command start" -l "cpus" -d "Number of CPUs allocated to the minikube VM (default 2)"
complete -c minikube -n "__minikube_using_command start" -l "disk-size" -d "Disk size allocated to the minikube VM (format: <number>[<unit>], where unit = b, k, m or g) (default \"20g\")"
complete -c minikube -n "__minikube_using_command start" -l "docker-env" -d "Environment variables to pass to the Docker daemon. (format: key=value)"
complete -c minikube -n "__minikube_using_command start" -l "extra-config" -d "A set of key=value pairs that describe configuration that may be passed to different components."
complete -c minikube -n "__minikube_using_command start" -l "feature-gates" -d "A set of key=value pairs that describe feature gates for alpha/experimental features."
complete -c minikube -n "__minikube_using_command start" -l "host-only-cidr" -d "The CIDR to be used for the minikube VM (only supported with Virtualbox driver) (default \"192.168.99.1/24\")"
complete -c minikube -n "__minikube_using_command start" -l "hyperv-virtual-switch" -d "The hyperv virtual switch name. Defaults to first found. (only supported with HyperV driver)"
complete -c minikube -n "__minikube_using_command start" -l "insecure-registry" -d "Insecure Docker registries to pass to the Docker daemon"
complete -c minikube -n "__minikube_using_command start" -l "iso-url" -d "Location of the minikube iso (default \"https://storage.googleapis.com/minikube/iso/minikube-v1.0.2.iso\")"
complete -c minikube -n "__minikube_using_command start" -l "keep-context" -d "This will keep the existing kubectl context and will create a minikube context."
complete -c minikube -n "__minikube_using_command start" -l "kubernetes-version" -d "The kubernetes version that the minikube VM will use (ex: v1.2.3) "
complete -c minikube -n "__minikube_using_command start" -l "kvm-network" -d "The KVM network name. (only supported with KVM driver) (default \"default\")"
complete -c minikube -n "__minikube_using_command start" -l "memory" -d "Amount of RAM allocated to the minikube VM (default 2048)"
complete -c minikube -n "__minikube_using_command start" -l "network-plugin" -d "The name of the network plugin"
complete -c minikube -n "__minikube_using_command start" -l "registry-mirror" -d "Registry mirrors to pass to the Docker daemon"
complete -c minikube -n "__minikube_using_command start" -l "vm-driver" -d "VM driver is one of: [virtualbox kvm] (default \"virtualbox\")"
complete -c minikube -n "__minikube_using_command status" -l "format" -d "Go template format string for the status output. The format for Go templates can be found here: https://golang.org/pkg/text/template/"
