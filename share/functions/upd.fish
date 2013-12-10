function upd --description "Move up in the directory tree"
      set up_dir ".."
      cd $up_dir
      commandline -f repaint
end