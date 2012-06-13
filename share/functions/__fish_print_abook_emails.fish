function __fish_print_abook_emails --description 'Print email addresses (abook)'
	abook --mutt-query "" | egrep -v '^\s*$'

end
