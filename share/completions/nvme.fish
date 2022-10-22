# NVME user space tooling for Linux
# See: https://github.com/linux-nvme/nvme-cli

set cmds list list-subsys id-ctrl id-ns id-ns-granularity id-ns-lba-format list-ns list-ctrl nvm-id-ctrl nvm-id-ns nvm-id-ns-lba-format primary-ctrl-caps list-secondary cmdset-ind-id-ns ns-descs id-nvmset id-uuid id-iocs id-domain list-endgrp create-ns delete-ns attach-ns detach-ns get-ns-id get-log telemetry-log fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log get-feature device-self-test self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log media-unit-stat-log supported-cap-config-log set-feature set-property get-property format fw-commit fw-download admin-passthru io-passthru security-send security-recv get-lba-status capacity-mgmt resv-acquire resv-register resv-release resv-report dsm copy flush compare read write write-zeroes write-uncor verify sanitize sanitize-log reset subsystem-reset ns-rescan show-regs discover connect-all connect disconnect disconnect-all config gen-hostnqn show-hostnqn gen-dhchap-key check-dhchap-key gen-tls-key check-tls-key dir-receive dir-send virt-mgmt rpmb lockdown dim version help

function __fish_print_nvme_from_nvme_cli
    # If jq is installed, we use the JSON output and parse it with jq.
    # Otherwise, we fallback to the "normal" output which doesn't make any
    # guarantees on the format.
    if command -q jq
        nvme list --output-format json |
            jq '.Devices[] | "\(.DevicePath)\t\(.ModelNumber) - \(.SerialNumber)"' --raw-output
    else
        nvme list --output-format normal | string match -r '^/dev/[^ \t]+'
    end
end

function __fish_print_nvme
    # We first try to get the list from the nvme command. If this doesn't yield
    # any results (e.g. when we are not root), we fallback to looking at file
    # names in /dev/.
    set -a list (__fish_print_nvme_from_nvme_cli)

    if set -q list[1]
        printf "%s\n" $list
    else
        string match -vr -- 'p[0-9]+$' /dev/nvme*
    end
end

complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list -d "List all NVMe devices and namespaces on machine"
complete -c nvme -n "__fish_seen_subcommand_from list" -s v -l verbose -d "Increase verbosity"

complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list-subsys -d "List nvme subsystems"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-ctrl -d "Send NVMe Identify Controller"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-ns -d "Send NVMe Identify Namespace"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-ns-granularity -d "Send NVMe Identify Namespace Granularity List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-ns-lba-format -d "Send NVMe Identify Namespace for the specified LBA Format index"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list-ns -d "Send NVMe Identify List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list-ctrl -d "Send NVMe Identify Controller List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a nvm-id-ctrl -d "Send NVMe Identify Controller NVM Command Set"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a nvm-id-ns -d "Send NVMe Identify Namespace NVM Command Set"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a nvm-id-ns-lba-format -d "Send NVMe Identify Namespace NVM Command Set for the specified LBA Format index"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a primary-ctrl-caps -d "Send NVMe Identify Primary Controller Capabilities"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list-secondary -d "List Secondary Controllers associated with a Primary Controller"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a cmdset-ind-id-ns -d "I/O Command Set Independent Identify Namespace"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a ns-descs -d "Send NVMe Namespace Descriptor List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-nvmset -d "Send NVMe Identify NVM Set List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-uuid -d "Send NVMe Identify UUID List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-iocs -d "Send NVMe Identify I/O Command Set"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a id-domain -d "Send NVMe Identify Domain List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a list-endgrp -d "Send NVMe Identify Endurance Group List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a create-ns -d "Creates a namespace with the provided parameters"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a delete-ns -d "Deletes a namespace from the controller"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a attach-ns -d "Attaches a namespace to requested controller(s)"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a detach-ns -d "Detaches a namespace from requested controller(s)"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a get-ns-id -d "Retrieve the namespace ID of opened block device"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a get-log -d "Generic NVMe get log, returns log in raw format"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a telemetry-log -d "Retrieve FW Telemetry log write to file"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a fw-log -d "Retrieve FW Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a changed-ns-list-log -d "Retrieve Changed Namespace List"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a smart-log -d "Retrieve SMART Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a ana-log -d "Retrieve ANA Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a error-log -d "Retrieve Error Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a effects-log -d "Retrieve Command Effects Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a endurance-log -d "Retrieve Endurance Group Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a predictable-lat-log -d "Retrieve Predictable Latency per Nvmset Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a pred-lat-event-agg-log -d "Retrieve Predictable Latency Event Aggregate Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a persistent-event-log -d "Retrieve Presistent Event Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a endurance-event-agg-log -d "Retrieve Endurance Group Event Aggregate Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a lba-status-log -d "Retrieve LBA Status Information Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a resv-notif-log -d "Retrieve Reservation Notification Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a boot-part-log -d "Retrieve Boot Partition Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a get-feature -d "Get feature and show the resulting value"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a device-self-test -d "Perform the necessary tests to observe the performance"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a self-test-log -d "Retrieve the SELF-TEST Log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a supported-log-pages -d "Retrieve the Supported Log pages details"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a fid-support-effects-log -d "Retrieve FID Support and Effects log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a mi-cmd-support-effects-log -d "Retrieve MI Command Support and Effects log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a media-unit-stat-log -d "Retrieve the configuration and wear of media units"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a supported-cap-config-log -d "Retrieve the list of Supported Capacity Configuration Descriptors"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a set-feature -d "Set a feature and show the resulting value"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a set-property -d "Set a property and show the resulting value"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a get-property -d "Get a property and show the resulting value"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a format -d "Format namespace with new block format"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a fw-commit -d "Verify and commit firmware to a specific slot"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a fw-download -d "Download new firmware"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a admin-passthru -d "Submit an arbitrary admin command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a io-passthru -d "Submit an arbitrary IO command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a security-send -d "Submit a Security Send command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a security-recv -d "Submit a Security Receive command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a get-lba-status -d "Submit a Get LBA Status command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a capacity-mgmt -d "Submit Capacity Management Command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a resv-acquire -d "Submit a Reservation Acquire"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a resv-register -d "Submit a Reservation Register"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a resv-release -d "Submit a Reservation Release"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a resv-report -d "Submit a Reservation Report"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a dsm -d "Submit a Data Set Management command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a copy -d "Submit a Simple Copy command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a flush -d "Submit a Flush command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a compare -d "Submit a Compare command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a read -d "Submit a read command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a write -d "Submit a write command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a write-zeroes -d "Submit a write zeroes command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a write-uncor -d "Submit a write uncorrectable command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a verify -d "Submit a verify command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a sanitize -d "Submit a sanitize command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a sanitize-log -d "Retrieve sanitize log"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a reset -d "Resets the controller"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a subsystem-reset -d "Resets the subsystem"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a ns-rescan -d "Rescans the NVME namespaces"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a show-regs -d "Shows the controller registers or properties"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a discover -d "Discover NVMeoF subsystems"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a connect-all -d "Discover and Connect to NVMeoF subsystems"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a connect -d "Connect to NVMeoF subsystem"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a disconnect -d "Disconnect from NVMeoF subsystem"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a disconnect-all -d "Disconnect from all connected NVMeoF subsystems"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a config -d "Configuration of NVMeoF subsystems"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a gen-hostnqn -d "Generate NVMeoF host NQN"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a show-hostnqn -d "Show NVMeoF host NQN"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a gen-dhchap-key -d "Generate NVMeoF DH-HMAC-CHAP host key"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a check-dhchap-key -d "Validate NVMeoF DH-HMAC-CHAP host key"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a gen-tls-key -d "Generate NVMeoF TLS PSK"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a check-tls-key -d "Validate NVMeoF TLS PSK"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a dir-receive -d "Submit a Directive Receive command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a dir-send -d "Submit a Directive Send command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a virt-mgmt -d "Manage Flexible Resources between Primary and Secondary Controller "
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a rpmb -d "Replay Protection Memory Block commands"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a lockdown -d "Submit Lockdown command"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a dim -d "Send Discovery Information Management command to a Discovery Controller"
complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a version -d "Show  version"

complete -c nvme -f -n "not __fish_seen_subcommand_from $cmds" -a help -d "Display help"
complete -c nvme -f -n "__fish_seen_subcommand_from help" -a "$cmds"

complete -c nvme -n "__fish_seen_subcommand_from list list-subsys id-ctrl id-ns list-ns list-ctrl nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain list-endgrp fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log get-lba-status resv-report sanitize-log show-regs discover connect list list-subsys id-ctrl id-ns list-ns list-ctrl nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain list-endgrp fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log get-lba-status resv-report sanitize-log show-regs discover connect list list-subsys id-ctrl id-ns list-ns list-ctrl nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain list-endgrp fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log get-lba-status resv-report sanitize-log show-regs discover connect list list-subsys id-ctrl id-ns list-ns list-ctrl nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain list-endgrp fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log get-lba-status resv-report sanitize-log show-regs discover connect" -x -s f -l output-format -d "Output format" -a "json\t normal\t"

complete -c nvme -n "__fish_seen_subcommand_from id-ctrl id-ns nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log self-test-log supported-log-pages fid-support-effects-log mi-cmd-support-effects-log get-lba-status resv-report sanitize-log show-regs discover" -x -s f -l output-format -d "Output format" -a "binary\t"
complete -c nvme -s h -l help -d "Display help"

complete -c nvme -f -n "__fish_seen_subcommand_from id-ctrl id-ns list-ns list-ctrl nvm-id-ctrl primary-ctrl-caps cmdset-ind-id-ns ns-descs id-nvmset id-iocs id-domain list-endgrp create-ns delete-ns attach-ns detach-ns get-ns-id get-log telemetry-log fw-log changed-ns-list-log smart-log ana-log error-log effects-log endurance-log predictable-lat-log pred-lat-event-agg-log persistent-event-log endurance-event-agg-log lba-status-log resv-notif-log boot-part-log get-feature device-self-test self-test-log supported-log-pages fid-support-effects-log set-feature set-property get-property format fw-commit fw-download security-send security-recv get-lba-status capacity-mgmt resv-acquire resv-register resv-release resv-report dsm flush sanitize sanitize-log reset subsystem-reset ns-rescan show-regs dir-receive dir-send rpmb" -d "NVME device" -a "(__fish_print_nvme)"
