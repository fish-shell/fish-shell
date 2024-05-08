set -l subcmds \
    show \
    update \
    default \
    toolchain \
    target \
    component \
    override \
    run \
    which \
    doc \
    man \
    self \
    set \
    # "completions" \
    help

set -l rustup_show \
    active-toolchain \
    home \
    profile \
    help

# rustup does not really expose a mechanism of retrieving a list of all valid components without the archs appended
# Just print the list of installable x86_64-unknown-linux-gnu components for everyone
function __rustup_components
    rustup component list | string match -r "\S+" | string replace -rf -- "-x86_64-unknown-linux-gnu.*" "" | sort -u
end

function __rustup_installed_components
    # If the user has supplied `--toolchain foo`, use it to filter the list
    set -l toolchain (commandline -j | string replace -rf ".*\s--toolchain\s+(\S+)\s.*" '$1')
    set -l toolchain_filter
    if string match -qr . -- $toolchain
        set toolchain_filter "--toolchain $toolchain"
    end

    rustup component list --installed $toolchain_filter | string replace -r -- (printf -- "-%s\n" (__rustup_targets) | string join "|") "" | sort -u

end

function __rustup_installable_components
    comm -2 -3 (__rustup_components | psub -F) (__rustup_installed_components | psub -F)
end

# All *valid* target triples, retrieved from source
function __rustup_triples
    # The list below must be kept sorted alphabetically
    printf "%s\n" \
        aarch64-apple-ios \
        aarch64-linux-android \
        aarch64-pc-windows-msvc \
        aarch64-unknown-cloudabi \
        aarch64-unknown-fuchsia \
        aarch64-unknown-linux-gnu \
        aarch64-unknown-linux-musl \
        aarch64-unknown-redox \
        armebv7r-none-eabi \
        armebv7r-none-eabihf \
        arm-linux-androideabi \
        arm-unknown-linux-gnueabi \
        arm-unknown-linux-gnueabihf \
        arm-unknown-linux-musleabi \
        arm-unknown-linux-musleabihf \
        armv5te-unknown-linux-gnueabi \
        armv5te-unknown-linux-musleabi \
        armv7-apple-ios \
        armv7-linux-androideabi \
        armv7r-none-eabi \
        armv7r-none-eabihf \
        armv7s-apple-ios \
        armv7-unknown-cloudabi-eabihf \
        armv7-unknown-linux-gnueabi \
        armv7-unknown-linux-gnueabihf \
        armv7-unknown-linux-musleabi \
        armv7-unknown-linux-musleabihf \
        asmjs-unknown-emscripten \
        i386-apple-ios \
        i586-pc-windows-msvc \
        i586-unknown-linux-gnu \
        i586-unknown-linux-musl \
        i686-apple-darwin \
        i686-linux-android \
        i686-pc-windows-gnu \
        i686-pc-windows-msvc \
        i686-unknown-cloudabi \
        i686-unknown-freebsd \
        i686-unknown-haiku \
        i686-unknown-linux-gnu \
        i686-unknown-linux-musl \
        i686-unknown-netbsd \
        le32-unknown-nacl \
        mips64el-unknown-linux-gnuabi64 \
        mips64-unknown-linux-gnuabi64 \
        mipsel-unknown-linux-gnu \
        mipsel-unknown-linux-musl \
        mipsel-unknown-linux-uclibc \
        mipsisa32r6el-unknown-linux-gnu \
        mipsisa32r6-unknown-linux-gnu \
        mipsisa64r6el-unknown-linux-gnuabi64 \
        mipsisa64r6-unknown-linux-gnuabi64 \
        mips-unknown-linux-gnu \
        mips-unknown-linux-musl \
        mips-unknown-linux-uclibc \
        msp430-none-elf \
        nvptx64-nvidia-cuda \
        powerpc64le-unknown-linux-gnu \
        powerpc64-unknown-linux-gnu \
        powerpc-unknown-linux-gnu \
        powerpc-unknown-linux-gnuspe \
        riscv32imac-unknown-none-elf \
        riscv32imc-unknown-none-elf \
        riscv32i-unknown-none-elf \
        riscv64gc-unknown-none-elf \
        riscv64imac-unknown-none-elf \
        s390x-unknown-linux-gnu \
        sparc64-unknown-linux-gnu \
        sparc64-unknown-netbsd \
        sparc-unknown-linux-gnu \
        sparcv9-sun-solaris \
        thumbv6m-none-eabi \
        thumbv7em-none-eabi \
        thumbv7em-none-eabihf \
        thumbv7m-none-eabi \
        thumbv7neon-linux-androideabi \
        thumbv7neon-unknown-linux-gnueabihf \
        "thumbv8m.base-none-eabi" \
        "thumbv8m.main-none-eabi" \
        "thumbv8m.main-none-eabihf" \
        wasm32-unknown-emscripten \
        wasm32-unknown-unknown \
        x86_64-apple-darwin \
        x86_64-apple-ios \
        x86_64-fortanix-unknown-sgx \
        x86_64-linux-android \
        x86_64-pc-solaris \
        x86_64-pc-windows-gnu \
        x86_64-pc-windows-msvc \
        x86_64-rumprun-netbsd \
        x86_64-sun-solaris \
        x86_64-unknown-bitrig \
        x86_64-unknown-cloudabi \
        x86_64-unknown-dragonfly \
        x86_64-unknown-freebsd \
        x86_64-unknown-fuchsia \
        x86_64-unknown-haiku \
        x86_64-unknown-linux-gnu \
        x86_64-unknown-linux-gnux32 \
        x86_64-unknown-linux-musl \
        x86_64-unknown-netbsd \
        x86_64-unknown-openbsd \
        x86_64-unknown-redox
end

# Given n arguments, return the longest suffix common to all
function __rustup_common_suffix
    if test (count $argv) -le 1
        return
    end

    set -l index -1
    set -l min_length 65335
    for arg in $argv
        if test (string length "$arg") -lt $min_length
            set min_length (string length "$arg")
        end
    end

    set -l length 1
    set -l suffix
    set -l done 0
    while test $done -eq 0 -a $length -le $min_length
        set -l match (string match -r -- ".{$length}\$" "$argv[1]")
        for arg in $argv[2..-1]
            set -l value (string match -r -- ".{$length}\$" "$arg")
            if test "$value" = "$match"
                continue
            else if string match -qr -- . "$suffix"
                set done 1
                break
            else
                # No common suffix
                return 1
            end
        end
        set suffix "$match"
        set length (math $length + 1)
    end

    printf "%s\n" "$suffix"
end

# Strip result of __rustup_common_suffix
function __rustup_strip_common_suffix
    set -l suffix (__rustup_common_suffix $argv)
    printf "%s\n" $argv | string replace -r -- (string escape --style=regex -- "$suffix")\$ ""
end

# Strip result of __rustup_common_suffix, but require suffix to begin at a literal -
function __rustup_strip_common_suffix_strict
    set -l suffix (__rustup_common_suffix $argv | string match -r -- "-.*")
    printf "%s\n" $argv | string replace -r -- (string escape --style=regex -- "$suffix")\$ ""
end

function __rustup_all_toolchains
    set -l __rustup_channels beta stable nightly
    printf "%s\n" $__rustup_channels
    printf "%s\n" $__rustup_channels-(__rustup_triples)
end

# All valid toolchains excluding installed
function __rustup_installable_toolchains
    comm -2 -3 (__rustup_all_toolchains | psub -F) (printf "%s\n" $__rustup_toolchains | psub -F) \
        # Ignore warnings about lists not being in sorted order, as we are aware of their contents
        2>/dev/null
end

function __rustup_targets
    rustup target list | string replace -rf "^(\S+).*" '$1'
end

function __rustup_installed_targets
    rustup target list | string replace -rf "^(\S+) \(installed\)" '$1'
end

# All valid targets excluding installed
function __rustup_installable_targets
    comm -2 -3 (__rustup_targets | psub -F) (__rustup_installed_targets | psub -F) \
        # Ignore warnings about lists not being in sorted order, as we are aware of their contents
        2>/dev/null
end

# Trim trailing attributes, e.g. "rust-whatever (default)" -> "rust-whatever"
set -l __rustup_toolchains (rustup toolchain list | string replace -r "\s+.*" '')
# By default, the long name of a toolchain is used (e.g. nightly-x86_64-unknown-linux-gnu),
# but a shorter version can be used if it is unambiguous.
set -l __rustup_toolchains_short (__rustup_strip_common_suffix_strict $__rustup_toolchains)

set -l rustup_profiles minimal default complete

complete -c rustup -n __fish_should_complete_switches -s v -l verbose
complete -c rustup -n __fish_should_complete_switches -s h -l help
complete -c rustup -n __fish_should_complete_switches -s V -l version

complete -c rustup -n "__fish_is_nth_token 1" -xa "$subcmds"

complete -c rustup -n "__fish_prev_arg_in default" -xa "$__rustup_toolchains_short $__rustup_toolchains"
complete -c rustup -n "__fish_prev_arg_in toolchain" -xa "add install list remove uninstall link help"
complete -c rustup -n "__fish_prev_arg_in target" -xa "list add install remove uninstall help"
complete -c rustup -n "__fish_prev_arg_in component" -xa "list add install remove uninstall help"
complete -c rustup -n "__fish_prev_arg_in override" -xa "list set unset help"
complete -c rustup -n "__fish_prev_arg_in run" -xa "$__rustup_toolchains_short $__rustup_toolchains"
complete -c rustup -n "__fish_prev_arg_in self" -xa "update remove uninstall upgrade-data help"
complete -c rustup -n "__fish_prev_arg_in set" -xa "default-host profile help"

complete -c rustup -n "__fish_seen_subcommand_from toolchain; and __fish_prev_arg_in remove uninstall" \
    -xa "$__rustup_toolchains $__rustup_toolchains_short"
complete -c rustup -n "__fish_seen_subcommand_from toolchain; and __fish_prev_arg_in add install" \
    -xa "(__rustup_installable_toolchains)"

complete -c rustup -n "__fish_seen_subcommand_from component; and __fish_prev_arg_in remove uninstall" \
    -xa "(__rustup_installed_components)"
complete -c rustup -n "__fish_seen_subcommand_from component; and __fish_prev_arg_in add install" \
    -xa "(__rustup_installable_components)"

complete -c rustup -n "__fish_seen_subcommand_from target; and __fish_prev_arg_in add install" \
    -xa "(__rustup_installable_targets)"
complete -c rustup -n "__fish_seen_subcommand_from target; and __fish_prev_arg_in remove uninstall" \
    -xa "(__rustup_installed_targets)"

complete -c rustup -n "__fish_seen_subcommand_from show;" -xa "$rustup_show"

complete -c rustup -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in default-host" \
    -xa "(__rustup_triples)"

complete -c rustup -n "__fish_seen_subcommand_from set; and __fish_prev_arg_in profile" \
    -xa "$rustup_profiles"

# Global argument completions where valid
complete -c rustup -n "__fish_prev_arg_in --toolchain" -xa "$__rustup_toolchains_short $__rustup_toolchains"

complete -f -c rustup -n '__fish_seen_subcommand_from doc' -a '(__fish_rustup_docs_primitives)'
complete -f -c rustup -n '__fish_seen_subcommand_from doc' -a '(__fish_rustup_docs_keywords)'
complete -f -c rustup -n '__fish_seen_subcommand_from doc' -a '(__fish_rustup_docs_macros)'
complete -f -c rustup -n '__fish_seen_subcommand_from doc' -a '(__fish_rustup_docs_modules)'

function __fish_rustup_docs_modules
    set -l doc_path (__rustup_doc_path)
    command find "$doc_path"/{std,core,alloc} -name index.html \
        | string replace --regex "$doc_path/(.+)/index\.html" '$1\tModule' \
        | string replace --all / ::
end

function __fish_rustup_docs_keywords
    set -l doc_path (__rustup_doc_path)
    string replace --regex "$doc_path/std/keyword\.(.+)\.html" '$1\tKeyword' "$doc_path"/std/keyword.*.html
end

function __fish_rustup_docs_primitives
    set -l doc_path (__rustup_doc_path)
    string replace --regex "$doc_path/std/primitive\.(.+)\.html" '$1\tPrimitive' "$doc_path"/std/primitive.*.html
end

function __fish_rustup_docs_macros
    set -l doc_path (__rustup_doc_path)
    string replace --regex "$doc_path/" '' "$doc_path"/{std,core,alloc}/macro.*.html \
        | string replace --regex 'macro\.(.+)\.html' '$1\tMacro' \
        | string replace --all / ::
end

function __rustup_doc_path
    rustup doc --path | string replace --regex '/[^/]*$' ''
end
