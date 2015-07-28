complete -c python2 -w python
# Override this to use python2 instead of python
complete -c python2 -s m -d 'Run library module as a script (terminates option list)' -xa "( find /usr/lib/(eval python2 -V 2>| sed 's/ //; s/\..\$//; s/P/p/') \$PYTHONPATH -maxdepth 1 -name '*.py' -printf '%f\n' | sed 's/.py//')"
