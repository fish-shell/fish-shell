function __fish_print_zypp_repos -d "Print repositories of zypper package manager"
    sed -n '/\[.*\]/s/\[\(.*\)\]/\1/gp' /etc/zypp/repos.d/*.repo
end
