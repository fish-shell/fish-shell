complete -c python2 -w python
# Override this to use python2 instead of python
complete -c python2 -s m -d 'Run library module as a script (terminates option list)' -xa '(python2 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'
