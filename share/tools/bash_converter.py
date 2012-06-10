#!/usr/bin/env python
IN_WORD, IN_SINGLE_QUOTES, IN_DOUBLE_QUOTES = 0, 1, 2

global IN_BACKTICK
IN_BACKTICK = False

#Find outside quotes
def contains_outside_quotes(input, ch):
	in_quotes = False

	i = 0
	
	while i < len(input):
		if input[i] == ch and in_quotes == False:
			return True
		elif input[i] == '\\' and in_quotes == True:
			i = i+1	
		elif input[i] == '"':
			in_quotes = not in_quotes
		elif input[i] == "'" and not in_quotes:
			i = i + 1
			while input[i] != "'":
				i = i + 1	

		i = i + 1

	return False 

#Replace characters outside double quotes
def replace_outside_quotes(input, oldchar, newchar, change_all = True):
	in_quotes = False
	newstr = [] 

	i = 0
	
	while i < len(input):
		if input[i] == oldchar and in_quotes == False:
			newstr.append(newchar)
			i = i+1

			if change_all == True:
				continue	
			else:
				while i < len(input):
					newstr.append(input[i])
					i = i + 1
				#Break loop and return all the characters in list by joining them
				break	
		elif input[i] == '\\' and in_quotes == True:
			newstr.append(input[i])
			i = i+1	
		elif input[i] == '"':
			in_quotes=not in_quotes
		elif input[i] == "'" and not in_quotes:
			newstr.append(input[i])
			i = i + 1
			while input[i] != "'":
				newstr.append(input[i])
				i = i + 1	

		newstr.append(input[i])
		i = i + 1

	return ''.join(newstr)	

def export_handler(tokens):
#    print "Export Handler: ", tokens
    if contains_outside_quotes(tokens[0], "="):
       tokens[0] = replace_outside_quotes(tokens[0], "=", " ", False) 
    args = ' '.join(tokens) 
    return "set -gx " + args 

global bash_builtins

bash_builtins = {
    "export" : export_handler 
    }

def process_builtin(tokens):
    builtin_name = tokens[0]
#    print "Builtin name is: ", builtin_name
    return (bash_builtins[builtin_name])(tokens[1:])
    
def process_command(tokens):
    cmd = ' '.join(tokens)
    return cmd 

def parse_input(input, output_buff):
    i = 0
    state = IN_WORD

    tokens = []
    current_value = ""

    while (i<len(input)):
        if state == IN_WORD:

            if input[i] == " ":
                if len(current_value) > 0:
                    tokens.append(current_value)
                    current_value = ""
           
            elif input[i] == "'":
                state = IN_SINGLE_QUOTES
                current_value = current_value + input[i]
                
            elif input[i] == '"':
                state = IN_DOUBLE_QUOTES
                current_value = current_value + input[i]
            elif input[i] == "`":
                global IN_BACKTICK
                if IN_BACKTICK == True:
                    current_value = current_value + ")"
                else:
                    current_value = current_value + "(" 

                IN_BACKTICK = not IN_BACKTICK

            elif input[i] == ";":
                if len(current_value) > 0:
                    tokens.append(current_value)
#                    print tokens
                if contains_outside_quotes(tokens[0], "="):
                    tokens[0] = replace_outside_quotes(tokens[0], "=", " ", False) 
                    args = ' '.join(tokens) 
                    output_buff += "\n\tset " + args 
                elif tokens[0] in bash_builtins.keys():
                    output_buff += "\n\t" + process_builtin(tokens)
                else:
                    output_buff += "\n\t" + process_command(tokens)

                current_value = ""
                del tokens[:]
            elif input[i] == "&":
               if input[i+1] == "&":
                   current_value = current_value + "and"
                   i = i + 1
               else:
                   current_value = current_value + input[i]

            elif input[i] == "{":
                value_len = len(current_value)
                if (value_len > 0 and current_value[value_len - 1] == "$"):
                    current_value = current_value[:value_len - 1]
                    current_value = current_value + "{"
                    current_value = current_value + "$"
            else:
                current_value = current_value + input[i]
          
        elif state == IN_DOUBLE_QUOTES:
            current_value = current_value + input[i]
#            print current_value
            if input[i] == '"':
                state = IN_WORD
                tokens.append(current_value)
                current_value = ""

            #Let's ignore escaped characters
            elif input[i] == "\\":
#                print "In backslash"
                i = i + 1
                current_value = current_value + input[i]

        elif state == IN_SINGLE_QUOTES:
            current_value = current_value + input[i]
            if input[i] == "'":
                state = IN_WORD
                tokens.append(current_value)
                current_value = ""

        i = i + 1

    if len(current_value) > 0:
        tokens.append(current_value)

#    print tokens

    if contains_outside_quotes(tokens[0], "="):
        tokens[0] = replace_outside_quotes(tokens[0], "=", " ", False) 
        args = ' '.join(tokens) 
        output_buff += "\n\tset " + args
    elif tokens[0] in bash_builtins.keys():
        output_buff += "\n\t" + process_builtin(tokens)
    else:
        output_buff += "\n\t" + process_command(tokens)

    return output_buff

if __name__ == "__main__":
    import sys 
    data = sys.stdin.read()
#    data = 'echo "This is a message\\"'
#    data = "export a=b; export c=d; `echo 'HI'`; a=b; echo ${abc} "
    parse_input(data)

