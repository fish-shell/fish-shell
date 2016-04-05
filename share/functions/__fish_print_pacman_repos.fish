function __fish_print_pacman_repos --description "Print the repositories configured for arch's pacman package manager"
	string replace -r -a "\[(.+)\]" "\1" < /etc/pacman.conf | string match -r -v "^#|options"
end
