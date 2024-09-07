complete -c cascadia -s h -l help -d 'Show help'

complete -c cascadia -s i -l in -d "Specify the html/xml file to read from or stdin"
complete -c cascadia -s o -l out -d "Specify the output file or stdout"
complete -c cascadia -s c -l css -d "Specify the CSS selectors"
complete -c cascadia -s t -l text -d "Specify the text output for none-block selection mode"
complete -c cascadia -s R -l Raw -d "Use the raw text output"
complete -c cascadia -s p -l piece -d "Specify CSS sub-selectors within --css to split that block up"
complete -c cascadia -s d -l delimiter -d "Specify the delimiter for pieces csv output"
complete -c cascadia -s w -l wrap-html -d "Wrap up the output with html tags"
complete -c cascadia -s y -l style -d "Style component within the wrapped html head"
complete -c cascadia -s b -l base -d "Specify the base href tag used in the wrapped up html"
complete -c cascadia -s q -l quiet -d "Do not show any output"
