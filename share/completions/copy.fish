function __copy_generate_args --description 'Function to generate args'
  if not __fish_seen_argument --windows 'a' --windows 'b'
    echo -e '/a\tIndicate an ASCII text file
/b\tIndicate a binary file'
  end

  if not __fish_seen_argument --windows 'y' --windows '-y'
    echo -e '/y=\tHide prompts to confirm that you want to overwrite an existing destination file
/-y\tShow prompts to confirm that you want to overwrite an existing destination file'
  end

  echo -e '/d\tAllow the encrypted files being copied to be saved as decrypted files at the destination
/v\tVerify that new files are written correctly
/n\tUse a short file name, if available
/z\tCopy networked files in restartable mode
/?\tShow help'
end

complete --command copy --no-files --arguments '(__copy_generate_args)'
