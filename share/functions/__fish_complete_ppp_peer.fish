function __fish_complete_ppp_peer --description 'Complete isp name for pon/poff'
    find /etc/ppp/peers/ -type f -printf '%f\n'

end
