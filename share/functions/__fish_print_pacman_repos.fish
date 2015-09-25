function __fish_print_pacman_repos --description "Print the repositories configured for arch's pacman package manager"
	sed -n -e 's/\[\(.\+\)\]/\1/p' /etc/pacman.conf | grep -v "#\|options"
end
