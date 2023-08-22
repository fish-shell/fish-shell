function __fish_print_apt_packages
    argparse --name=__fish_print_packages i/installed -- $argv
    or return

    switch (commandline -ct)
        case '-**'
            return
    end

    set -l search_term (commandline -ct | string replace -ar '[\'"\\\\]' '' | string lower)

    if ! test -f /var/lib/dpkg/status
        return 1
    end

    if not set -q _flag_installed
        # Do not generate the cache as apparently sometimes this is slow.
        # http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=547550
        # (It is safe to use `sed -r` here as we are guaranteed to be on a GNU platform
        # if apt-cache was found.)
        # Uses the UTF-8/ASCII record separator (0x1A) character.
        #
        # Note: This can include "Description:" fields which we need to include,
        # "Description-en_GB" (or another locale code) fields which we need to include
        # as well as "Description-md5" fields which we absolutely do *not* want to include
        # The regex doesn't allow numbers, so unless someone makes a hash algorithm without a number
        # in the name, we're safe. (yes, this should absolutely have a better format).
        #
        # aptitude has options that control the output formatting, but is orders of magnitude slower
        #
        # sed could probably do all of the heavy lifting here, but would be even less readable
        #
        # The `head -n 500` causes us to stop once we have 500 lines. We do it after the `sed` because
        # Debian package descriptions can be extremely long and are hard-wrapped: texlive-latex-extra
        # has about 2700 lines on Debian 11.
        apt-cache --no-generate show '.*'(commandline -ct)'.*' 2>/dev/null | sed -r '/^(Package|Description-?[a-zA-Z_]*):/!d;s/Package: (.*)/\1\t/g;s/Description-?[^:]*: (.*)/\1\x1a/g' | head -n 2500 | string join "" | string replace --all --regex \x1a+ \n | uniq
        return 0
    else
        # Do not not use `apt-cache` as it is sometimes inexplicably slow (by multiple orders of magnitude).
        awk '
BEGIN {
  FS=": "
}

/^Package/ {
  pkg=$2
}

/^Status/ {
  installed=0
  if ($2 ~ /(^|\s)installed/) {
    installed=1
  }
}

/^Description(-[a-zA-Z]+)?:/ {
  desc=$2
  if (installed == 1 && index(pkg, "'$search_term'") > 0) {
    print pkg "\t" desc
    installed=0 # Prevent multiple description translations from being printed
  }
}' </var/lib/dpkg/status
    end
end
