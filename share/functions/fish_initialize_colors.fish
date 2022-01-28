function fish_initialize_colors -d "Set default color uvars"
	# Regular syntax highlighting colors
        __init_uvar fish_color_normal normal
        __init_uvar fish_color_command blue
        __init_uvar fish_color_param cyan
        __init_uvar fish_color_redirection cyan --bold
        __init_uvar fish_color_comment red
        __init_uvar fish_color_error brred
        __init_uvar fish_color_escape brcyan
        __init_uvar fish_color_operator brcyan
        __init_uvar fish_color_end green
        __init_uvar fish_color_quote yellow
        __init_uvar fish_color_autosuggestion 555 brblack
        __init_uvar fish_color_user brgreen
        __init_uvar fish_color_host normal
        __init_uvar fish_color_host_remote yellow
        __init_uvar fish_color_valid_path --underline
        __init_uvar fish_color_status red

        __init_uvar fish_color_cwd green
        __init_uvar fish_color_cwd_root red

        # Background color for search matches
        __init_uvar fish_color_search_match --background=111

        # Background color for selections
        __init_uvar fish_color_selection white --bold --background=brblack

        # XXX fish_color_cancel was added in 2.6, but this was added to post-2.3 initialization
        # when 2.4 and 2.5 were already released
        __init_uvar fish_color_cancel -r

        # Pager colors
        __init_uvar fish_pager_color_prefix cyan --bold --underline
        __init_uvar fish_pager_color_completion normal
        __init_uvar fish_pager_color_description B3A06D yellow -i
        __init_uvar fish_pager_color_progress brwhite --background=cyan
        __init_uvar fish_pager_color_selected_background -r

        #
        # Directory history colors
        #
        __init_uvar fish_color_history_current --bold
end
