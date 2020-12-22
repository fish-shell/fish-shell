function __fish_print_apt_packages
    argparse --name=__fish_print_packages i/installed -- $argv
    or return

    switch (commandline -ct)
        case '-**'
            return
    end

    type -q -f apt-cache || return 1
    if not set -q _flag_installed
        # Do not generate the cache as apparently sometimes this is slow.
        # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=547550
        # (It is safe to use `sed -r` here as we are guaranteed to be on a GNU platform
        # if apt-cache was found. Using unicode reserved range in `fish/tr` and the
        # little-endian bytecode equivalent in `sed`. Supports localization.)
        #
        # Note: This can include "Description:" fields which we need to include,
        # "Description-en_GB" (or another locale code) fields which we need to include
        # as well as "Description-md5" fields which we absolutely do *not* want to include
        # The regex doesn't allow numbers, so unless someone makes a hash algorithm without a number in the name,
        # we're safe. (yes, this should absolutely have a better format).
        apt-cache --no-generate show '.*'(commandline -ct)'.*' 2>/dev/null | sed -r '/^(Package|Description-?[a-zA-Z_]*):/!d;s/Package: (.*)/\1\t/g;s/Description-?[^:]*: (.*)/\1\xee\x80\x80\x0a/g' | tr -d \n | tr -s \uE000 \n | uniq
        return 0
    else
        set -l packages (dpkg --get-selections | string replace -fr '(\S+)\s+install' "\$1" | string match -e (commandline -ct))
        apt-cache --no-generate show $packages 2>/dev/null | sed -r '/^(Package|Description-?[a-zA-Z_]*):/!d;s/Package: (.*)/\1\t/g;s/Description-?[^:]*: (.*)/\1\xee\x80\x80\x0a/g' | tr -d \n | tr -s \uE000 \n | uniq

        return 0
    end
end
