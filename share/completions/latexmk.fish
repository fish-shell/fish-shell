complete -c latexmk -x -a "(__fish_complete_suffix (commandline -ct) .tex '(La)TeX file')"
complete -c latexmk -o dvi -d "Make dvi"
complete -c latexmk -o ps  -d "Make ps dvi->ps"
complete -c latexmk -o pdf -d "Make pdf"
complete -c latexmk -o pdfps -d "Make pdf dvi->ps->pdf"
complete -c latexmk -o pdfdvi -d "Make pdf dvi->pdf"
complete -c latexmk -o commands -d "Print available commands"

#   -bibtex       - use bibtex when needed (default)
#   -bibtex-      - never use bibtex
#   -bibtex-cond  - use bibtex when needed, but only if the bib files exist
#   -bm <message> - Print message across the page when converting to postscript
#   -bi <intensity> - Set contrast or intensity of banner
#   -bs <scale> - Set scale for banner
#   -commands  - list commands used by latexmk for processing files
#   -c     - clean up (remove) all nonessential files, except dvi, ps and pdf files.
#   -C     - clean up (remove) all nonessential files
#   -CA     - clean up (remove) all nonessential files.  Equivalent to -C option.
#   -CF     - Remove file of database of file information before doing other actions
#   -cd    - Change to directory of source file when processing it
#   -cd-   - Do NOT change to directory of source file when processing it
#   -dependents or -deps - Show list of dependent files after processing
#   -dependents- or -deps- - Do not show list of dependent files
#   -deps-out=file - Set name of output file for dependency list, and turn on showing of dependency list
#   -dF <filter> - Filter to apply to dvi file
#   -dvi   - generate dvi
#   -dvi-  - turn off required dvi
#   -e <code> - Execute specified Perl code (as part of latexmk start-up code)
#   -f     - force continued processing past errors
#   -f-    - turn off forced continuing processing past errors
#   -gg    - Super go mode: clean out generated files before processing
#   -g     - process regardless of file timestamps
#   -g-    - Turn off -g
#   -h     - print help
#   -help  - print help
#   -jobname=STRING - set basename of output file(s) to STRING.
#   -l     - force landscape mode
#   -l-    - turn off -l
#   -latex=<program> - set program used for latex.  (replace '<program>' by the program name)
#   -new-viewer    - in -pvc mode, always start a new viewer
#   -new-viewer-   - in -pvc mode, start a new viewer only if needed
#   -nobibtex      - never use bibtex
#   -nodependents  - Do not show list of dependent files after processing
#   -norc          - omit automatic reading of system, user and project rc files
#   -output-directory=dir or -outdir=dir - set name of directory for output files
#   -pdf   - generate pdf by pdflatex
#   -pdfdvi - generate pdf by dvipdf
#   -pdflatex=<program> - set program used for pdflatex.
#                      (replace '<program>' by the program name)
#   -pdfps - generate pdf by ps2pdf
#   -pdf-  - turn off pdf
#   -ps    - generate postscript
#   -ps-   - turn off postscript
#   -pF <filter> - Filter to apply to postscript file
#   -p     - print document after generating postscript.
#            (Can also .dvi or .pdf files -- see documentation)
#   -print=dvi     - when file is to be printed, print the dvi file
#   -print=ps      - when file is to be printed, print the ps file (default)
#   -print=pdf     - when file is to be printed, print the pdf file
#   -pv    - preview document.  (Side effect turn off continuous preview)
#   -pv-   - turn off preview mode
#   -pvc   - preview document and continuously update.  (This also turns
#                on force mode, so errors do not cause latexmk to stop.)
#            (Side effect: turn off ordinary preview mode.)
#   -pvc-  - turn off -pvc
#   -quiet    - silence progress messages from called programs
#   -r <file> - Read custom RC file
#               (N.B. This file could override options specified earlier
#               on the command line.)
#   -recorder - Use -recorder option for (pdf)latex
#               (to give list of input and output files)
#   -recorder- - Do not use -recorder option for (pdf)latex
#   -rules    - Show list of rules after processing
#   -rules-   - Do not show list of rules after processing
#   -silent   - silence progress messages from called programs
#   -time     - show CPU time used
#   -time-    - don't show CPU time used
#   -use-make - use the make program to try to make missing files
#   -use-make- - don't use the make program to try to make missing files
#   -v        - display program version
#   -verbose  - display usual progress messages from called programs
#   -version      - display program version
#   -view=default - viewer is default (dvi, ps, pdf)
#   -view=dvi     - viewer is for dvi
#   -view=none    - no viewer is used
#   -view=ps      - viewer is for ps
#   -view=pdf     - viewer is for pdf
#   filename = the root filename of LaTeX document
#
#-p, -pv and -pvc are mutually exclusive
#-h, -c and -C override all other options.
#-pv and -pvc require one and only one filename specified
#All options can be introduced by '-' or '--'.  (E.g., --help or -help.)
