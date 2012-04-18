complete -c latexmk -x -a "(__fish_complete_suffix (commandline -ct) .tex '(La)TeX file')"
complete -c latexmk -o dvi -d "Make dvi"
complete -c latexmk -o ps  -d "Make ps dvi->ps"
complete -c latexmk -o pdf -d "Make pdf"
complete -c latexmk -o pdfps -d "Make pdf dvi->ps->pdf"
complete -c latexmk -o pdfdvi -d "Make pdf dvi->pdf"

