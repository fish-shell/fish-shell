# bluetoothctl enables to manage bluetooth devices and controllers.

set -l cmds list show select devices paired-devices system-alias reset-alias power pairable discoverable discoverable-timeout agent default-agent advertise set-alias scan info pair cancel-pairing trust untrust block unblock remove connect disconnect scan.uuids scan.service scan.manufacturer scan.data scan.discoverable scan.discoverable-timeout scan.tx-power scan.name scan.appearance scan.duration scan.timeout scan.secondary scan.clear gatt.list-attributes gatt.select-attribute gatt.attribute-info gatt.read gatt.write gatt.acquire-write gatt.release-write gatt.acquire-notify gatt.release-notify gatt.notify gatt.clone gatt.register-application gatt.unregister-application gatt.register-service gatt.unregister-service gatt.register-includes gatt.unregister-includes gatt.register-characteristic gatt.unregister-characteristic gatt.register-descriptor gatt.unregister-descriptor advertise.uuids advertise.service advertise.manufacturer advertise.data advertise.discoverable advertise.discoverable-timeout advertise.tx-power advertise.name advertise.appearance advertise.duration advertise.timeout advertise.secondary advertise.clear

function __fish_list_bluetoothctl_devices
    # Output of `bluetoothctl devices`:
    # Device 01:23:34:56:89:AB Name1
    # Device 01:23:34:56:89:AC Name2
    bluetoothctl devices 2>/dev/null | string replace -r "^Device " "" | string replace " " \t
end

function __fish_list_bluetoothctl_controllers
    # Output of `bluetoothctl list`:
    # Controller 01:23:34:56:89:AB Name1 [default]
    # Controller 01:23:34:56:89:AC Name2
    bluetoothctl list 2>/dev/null | string replace -r "^Controller " "" | string replace " " \t
end

complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a list -d "List available controllers"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a show -d "Controller information"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a select -d "Select default controller"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a devices -d "List available devices"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a paired-devices -d "List paired devices"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a system-alias -d "Set controller alias"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a reset-alias -d "Reset controller alias"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a power -d "Set controller power"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a pairable -d "Set controller pairable mode"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a discoverable -d "Set controller discoverable mode"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a discoverable-timeout -d "Set discoverable timeout"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a agent -d "Enable/disable agent with given capability"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a default-agent -d "Set agent as the default one"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise -d "Enable/disable advertising with given type"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a set-alias -d "Set device alias"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan -d "Scan for devices"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a info -d "Device information"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a pair -d "Pair with device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a cancel-pairing -d "Cancel pairing with device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a trust -d "Trust device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a untrust -d "Untrust device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a block -d "Block device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a unblock -d "Unblock device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a remove -d "Remove device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a connect -d "Connect device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a disconnect -d "Disconnect device"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.uuids -d "Set/Get advertise uuids"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.service -d "Set/Get advertise service data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.manufacturer -d "Set/Get advertise manufacturer data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.data -d "Set/Get advertise data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.discoverable -d "Set/Get advertise discoverable"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.discoverable-timeout -d "Set/Get advertise discoverable timeout"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.tx-power -d "Show/Enable/Disable TX power to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.name -d "Configure local name to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.appearance -d "Configure custom appearance to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.duration -d "Set/Get advertise duration"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.timeout -d "Set/Get advertise timeout"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.secondary -d "Set/Get advertise secondary channel"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a scan.clear -d "Clear advertise config"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.list-attributes -d "List attributes"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.select-attribute -d "Select attribute"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.attribute-info -d "Select attribute"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.read -d "Read attribute value"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.write -d "Write attribute value"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.acquire-write -d "Acquire Write file descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.release-write -d "Release Write file descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.acquire-notify -d "Acquire Notify file descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.release-notify -d "Release Notify file descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.notify -d "Notify attribute value"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.clone -d "Clone a device or attribute"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.register-application -d "Register profile to connect"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.unregister-application -d "Unregister profile"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.register-service -d "Register application service"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.unregister-service -d "Unregister application service"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.register-includes -d "Register as Included service in"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.unregister-includes -d "Unregister Included service"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.register-characteristic -d "Register application characteristic"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.unregister-characteristic -d "Unregister application characteristic"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.register-descriptor -d "Register application descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a gatt.unregister-descriptor -d "Unregister application descriptor"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.uuids -d "Set/Get advertise uuids"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.service -d "Set/Get advertise service data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.manufacturer -d "Set/Get advertise manufacturer data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.data -d "Set/Get advertise data"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.discoverable -d "Set/Get advertise discoverable"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.discoverable-timeout -d "Set/Get advertise discoverable timeout"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.tx-power -d "Show/Enable/Disable TX power to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.name -d "Configure local name to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.appearance -d "Configure custom appearance to be advertised"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.duration -d "Set/Get advertise duration"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.timeout -d "Set/Get advertise timeout"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.secondary -d "Set/Get advertise secondary channel"
complete -f -c bluetoothctl -n "not __fish_seen_subcommand_from $cmds" -a advertise.clear -d "Clear advertise config"

complete -f -c bluetoothctl -n "__fish_seen_subcommand_from pair trust untrust block unblock connect disconnect" -a "(__fish_list_bluetoothctl_devices)"
complete -f -c bluetoothctl -n "__fish_seen_subcommand_from show select" -a "(__fish_list_bluetoothctl_controllers)"
complete -f -c bluetoothctl -n "__fish_seen_subcommand_from power pairable discoverable agent advertize" -a "on off"
