function __ghoti_print_apt_packages
    argparse --name=__ghoti_print_packages i/installed -- $argv
    or return

    switch (commandline -ct)
        case '-**'
            return
    end

    set -l search_term (commandline -ct | string replace -ar '[\'"\\\\]' '' | string lower)

    if ! test -f /var/lib/dpkg/status
        return 1
    end

    # Do not not use `apt-cache` as it is sometimes inexplicably slow (by multiple orders of magnitude).
    if not set -q _flag_installed
        awk -e '
BEGIN {
  FS=": "
}

/^Package/ {
  pkg=$2
}

/^Description(-[a-zA-Z]+)?:/ {
  desc=$2
  if (index(pkg, "'$search_term'") > 0) {
    print pkg "\t" desc
  }
  pkg="" # Prevent multiple description translations from being printed
}' < /var/lib/dpkg/status
    else
        awk -e '
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
}' < /var/lib/dpkg/status
    end
end
