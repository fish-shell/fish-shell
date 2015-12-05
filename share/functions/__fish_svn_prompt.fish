# colour of the revision number to display in the prompt
set -g __fish_svn_prompt_color_revision yellow

# setting the prompt status separator character
set -g __fish_svn_prompt_char_separator "|"

# ==============================

# SVN status display variables
# these are paired in groups of two:
# 1. the character to display in the prompt
# 2. the colour that should be used to display the character
#
# these variables are user-configurable and can be set to customize the display output

set -g __fish_svn_prompt_char_added_display 'A'
set -g __fish_svn_prompt_char_added_color green

set -g __fish_svn_prompt_char_conflicted_display 'C'
set -g __fish_svn_prompt_char_conflicted_color --underline magenta

set -g __fish_svn_prompt_char_deleted_display 'D'
set -g __fish_svn_prompt_char_deleted_color red

set -g __fish_svn_prompt_char_ignored_display 'I'
set -g __fish_svn_prompt_char_ignored_color --bold yellow

set -g __fish_svn_prompt_char_modified_display 'M'
set -g __fish_svn_prompt_char_modified_color blue

set -g __fish_svn_prompt_char_replaced_display 'R'
set -g __fish_svn_prompt_char_replaced_color cyan

set -g __fish_svn_prompt_char_unversioned_external_display 'X'
set -g __fish_svn_prompt_char_unversioned_external_color --underline cyan

set -g __fish_svn_prompt_char_unversioned_display '?'
set -g __fish_svn_prompt_char_unversioned_color purple

set -g __fish_svn_prompt_char_missing_display '!'
set -g __fish_svn_prompt_char_missing_color yellow

set -g __fish_svn_prompt_char_versioned_obstructed_display '~'
set -g __fish_svn_prompt_char_versioned_obstructed_color magenta

set -g __fish_svn_prompt_char_locked_display 'L'
set -g __fish_svn_prompt_char_locked_color --bold red

set -g __fish_svn_prompt_char_scheduled_display '+'
set -g __fish_svn_prompt_char_scheduled_color --bold green

set -g __fish_svn_prompt_char_switched_display 'S'
set -g __fish_svn_prompt_char_switched_color --bold blue

set -g __fish_svn_prompt_char_token_present_display 'K'
set -g __fish_svn_prompt_char_token_present_color --bold cyan

set -g __fish_svn_prompt_char_token_other_display 'O'
set -g __fish_svn_prompt_char_token_other_color --underline purple

set -g __fish_svn_prompt_char_token_stolen_display 'T'
set -g __fish_svn_prompt_char_token_stolen_color --bold purple

set -g __fish_svn_prompt_char_token_broken_display 'B'
set -g __fish_svn_prompt_char_token_broken_color --bold magenta


# ==============================

function __fish_svn_prompt_parse_status --argument flag_status_string --description "helper function that does pretty formatting on svn status"
	# SVN status symbols
	# Do not change these! These are the expected characters that are output from `svn status`, they are taken from `svn help status`
	set -l __fish_svn_prompt_chars A C D I M R X \? ! ~ L + S K O T B
	# this sets up an array of all the types of status codes that could be returned.
	set -l __fish_svn_prompt_flag_names added conflicted deleted ignored modified replaced unversioned_external unversioned missing locked scheduled switched token_present token_other token_stolen token_broken
	# iterate over the different status types
	for flag_type in $__fish_svn_prompt_flag_names
		# resolve the name of the variable for the character representing the current status type
		set -l flag_index (contains -i $flag_type $__fish_svn_prompt_flag_names)
		# check to see if the status string for this column contains the character representing the current status type
		if test (echo $flag_status_string | grep -c $__fish_svn_prompt_chars[$flag_index]) -eq 1
			# if it does, then get the names of the variables for the display character and colour to format it with
			set -l flag_var_display __fish_svn_prompt_char_{$flag_type}_display
			set -l flag_var_color __fish_svn_prompt_char_{$flag_type}_color
			# set the colour and print display character, then restore to default display colour
			printf '%s%s%s' (set_color $$flag_var_color) $$flag_var_display (set_color normal)
		end
	end
end


function __fish_svn_prompt --description "Prompt function for svn"
	# if svn isn't installed then don't do anything
	if not command -s svn > /dev/null
		return 1
	end

	# make sure that this is a svn repo
	set -l checkout_info (command svn info ^/dev/null)
	if [ $status -ne 0 ];
		return
	end

	# get the current revision number
	printf '(%s%s%s' (set_color $__fish_svn_prompt_color_revision) (__fish_print_svn_rev) (set_color normal)

	# resolve the status of the checkout
	# 1. perform `svn status`
	# 2. remove extra lines that aren't necessary
	# 3. cut the output down to the first 7 columns, as these contain the information needed
	set -l svn_status_lines (command svn status | sed -e 's=^Summary of conflicts.*==' -e 's=^  Text conflicts.*==' -e 's=^  Property conflicts.*==' -e 's=^  Tree conflicts.*==' -e 's=.*incoming .* upon update.*==' | cut -c 1-7)

	# track the last column to contain a status flag
	set -l last_column 0

	# iterate over the 7 columns of output (the 7 columns are defined on `svn help status`)
	for col in (seq 7)
		# get the output for a particular column
		# 1. echo the whole status flag text
		# 2. cut out the current column of characters
		# 3. remove spaces and newline characters
		set -l column_status (printf '%s\n' $svn_status_lines | cut -c $col | tr -d ' \n')

		# check that the character count is not zero (this would indicate that there are status flags in this column)
		if [ (count $column_status) -ne 0 ];
			# we only want to display unique status flags (eg: if there are 5 modified files, the prompt should only show the modified status once)
			set -l column_unique_status (echo $column_status | sort | uniq)
			# parse the status flags for this column and create the formatting by calling out to the helper function
			set -l svn_status_flags (__fish_svn_prompt_parse_status $column_unique_status)

			# the default separator is empty
			set -l prompt_separator ""
			for index in (seq (math "$col - $last_column"))
				# the prompt separator variable has to be updated with the number of separators needed to represent empty status columns (eg: if a file has the status "A  +" then it should display as "A|||+" in the prompt)
				set prompt_separator $prompt_separator$__fish_svn_prompt_char_separator
			end

			# record that the current column was the last one printed to the prompt
			set last_column $col
			# print the separator string then the current column's status flags
			printf '%s%s' $prompt_separator $svn_status_flags
		end
	end

	# print the close of the svn status prompt
	printf ')'
end