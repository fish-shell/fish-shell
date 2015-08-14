complete -c python3 -w python
# Override this to use python2 instead of python
complete -c python3 -s m -d 'Run library module as a script (terminates option list)' -xa '(python3 -c "import pkgutil; print(\'\n\'.join([p[1] for p in pkgutil.iter_modules()]))")'

