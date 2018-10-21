function __fish_portage_print_available_categories --description 'Print all available categories'
    cat (__fish_portage_print_repository_paths)/profiles/categories 2>/dev/null
end
