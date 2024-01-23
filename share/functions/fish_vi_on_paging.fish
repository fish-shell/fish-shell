function fish_vi_on_paging
    function _fish_open_pager
        commandline -f complete
        if commandline --paging-mode
            commandline -f repaint
            set fish_bind_mode default
        end

    end
    bind --mode insert \t _fish_open_pager
end
