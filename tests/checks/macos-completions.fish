#RUN: %fish %s

# Mock macOS utilities so these completions can be checked on every platform.
function networksetup
    switch $argv[1]
        case -listallnetworkservices
            printf '%s\n' 'An asterisk denotes a disabled service.' Wi-Fi '*USB Ethernet'
        case -listallhardwareports
            printf '%s\n' 'Hardware Port: Wi-Fi' 'Device: en0'
        case -listlocations
            echo Automatic
        case -listBonds
            echo bond0
        case -listpppoeservices
            echo PPPoE
    end
end

function security
    switch $argv[1]
        case find-identity
            echo '  1) ABCDEF "Apple Development: Example (TEAM)"'
        case list-keychains
            echo '    "/Library/Keychains/System.keychain"'
        case help
            echo '    list-keychains                        Display keychains'
    end
end

function dscl
    echo example
end

function hdiutil
    echo diagnostic >&2
    echo '/dev/disk2s1 Apple_HFS /Volumes/Example'
end

function osalang
    echo AppleScript
end

function pkgutil
    echo org.example.package
end

function scutil
    echo '* (Disconnected) UUID "Example VPN" [VPN:IPSec]'
end

function systemsetup
    switch $argv[1]
        case -listtimezones
            printf '%s\n' 'Time Zones:' ' UTC'
        case -liststartupdisks
            echo /System/Library/CoreServices
    end
end

function system_profiler
    printf '%s\n' 'Available data types:' SPHardwareDataType
end

# Load the helpers without depending on whether the real commands are installed.
for cmd in codesign dseditgroup hdiutil networksetup osascript pkgutil scutil security sips systemsetup system_profiler fdesetup profiles sysadminctl
    status get-file completions/$cmd.fish | source
end

__fish_codesign_identities
# CHECK: Apple Development: Example (TEAM)
__fish_dseditgroup_groups
# CHECK: example
__fish_dseditgroup_users
# CHECK: example
__fish_hdiutil_devices
# CHECK: /dev/disk2s1
__fish_networksetup_services
# CHECK: Wi-Fi
# CHECK: USB Ethernet
__fish_networksetup_hardwareports
# CHECK: Wi-Fi
__fish_networksetup_devices
# CHECK: en0
__fish_networksetup_locations
# CHECK: Automatic
__fish_networksetup_bonds
# CHECK: bond0
__fish_networksetup_pppoe
# CHECK: PPPoE
__fish_osascript_languages
# CHECK: AppleScript
__fish_pkgutil_pkg_ids
# CHECK: org.example.package
__fish_pkgutil_group_ids
# CHECK: org.example.package
__fish_scutil_nc_services
# CHECK: Example VPN
__fish_security_keychains
# CHECK: /Library/Keychains/System.keychain
__fish_systemsetup_timezones
# CHECK: UTC
__fish_systemsetup_startupdisks
# CHECK: /System/Library/CoreServices

complete -C 'security help list-k' | string split -f1 \t
# CHECK: list-keychains
complete -C 'hdiutil attach -read' | string split -f1 \t
# CHECK: -readonly
# CHECK: -readwrite
complete -C 'sips -f ' | string split -f1 \t
# CHECK: horizontal
# CHECK: vertical
complete -C 'sips -s for' | string split -f1 \t
# CHECK: format
# CHECK: formatOptions
complete -C 'fdesetup enable -us' | string split -f1 \t
# CHECK: -user
# CHECK: -usertoadd
complete -C 'osascript -s ' | string split -f1 \t
# CHECK: e
# CHECK: h
# CHECK: he
# CHECK: ho
# CHECK: o
# CHECK: s
# CHECK: se
# CHECK: so
complete -C 'system_profiler -detailLevel ' | string split -f1 \t
# CHECK: basic
# CHECK: full
# CHECK: mini
complete -C 'system_profiler -detailLevel full ' | string split -f1 \t
# CHECK: SPHardwareDataType
complete -C 'system_profiler -timeout 10 ' | string split -f1 \t
# CHECK: SPHardwareDataType
set -qg __fish_scutil_v; and echo 'Leaked scutil loop variable'
set -qg __v; and echo 'Leaked profiles loop variable'
true

complete -C 'systemsetup -settimezone ' | string split -f1 \t
# CHECK: UTC
complete -C 'systemsetup -settimezone UTC '
complete -C 'systemsetup -setusingnetworktime ' | string split -f1 \t
# CHECK: off
# CHECK: on
complete -C 'systemsetup -setusingnetworktime on '
complete -C 'systemsetup -f -setremotelogin ' | string split -f1 \t
# CHECK: off
# CHECK: on

# -f is a leading switch, not an option taking an on/off argument.
complete -C 'systemsetup -' | string split -f1 \t | string match -- -f
# CHECK: -f
complete -C 'systemsetup -f -setusingnetworktime ' | string split -f1 \t
# CHECK: off
# CHECK: on
complete -C 'systemsetup -getdate -' | string split -f1 \t | string match -- -f
complete -C 'systemsetup -setremotelogin -' | string split -f1 \t | string match -- -f

# Authentication flags may precede the operation, and their values are not verbs.
complete -C 'sysadminctl -adminP' | string split -f1 \t
# CHECK: -adminPassword
complete -C 'sysadminctl -adminPassword '
complete -C 'sysadminctl -adminUser root -adminPassword -secret -secureTokenStatus ' | string split -f1 \t
# CHECK: example
complete -C 'sysadminctl -adminUser root -adminPassword - -addUser alice -fullN' | string split -f1 \t
# CHECK: -fullName
complete -C 'sysadminctl -guestAccount ' | string split -f1 \t
# CHECK: off
# CHECK: on
# CHECK: status
complete -C 'sysadminctl -guestAccount on '

mkdir sysadmin-home
complete -C 'sysadminctl -addUser alice -home sysadmin-' | string split -f1 \t
# CHECK: sysadmin-home/
complete -C 'sysadminctl -addUser alice -picture sysadmin-' | string split -f1 \t
# CHECK: sysadmin-home/
complete -C 'sysadminctl -adminUser root -adminPassword - -secureTokenOn alice -passw' | string split -f1 \t
# CHECK: -password
complete -C 'sysadminctl -adminUser root -adminPassword - -secureTokenOn alice -password '
true

# Preserve the existing values when completing comma-separated option arguments.
complete -C 'codesign --options runtime,li' | string split -f1 \t
# CHECK: runtime,library
# CHECK: runtime,linker-signed
complete -C 'codesign --preserve-metadata=identifier,en' | string split -f1 \t
# CHECK: --preserve-metadata=identifier,entitlements
complete -C 'codesign --strict=symlinks,si' | string split -f1 \t
# CHECK: --strict=symlinks,sideband
