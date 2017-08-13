function __fish_print_zypp_repos -d "Print repositories of zypper package manager"
    for repofile in /etc/zypp/repos.d/*.repo
        string replace -f -r '\[(.+)\]' '$1' < $repofile
    end
end
