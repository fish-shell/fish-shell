function __comp_generate_args --description 'Function to generate args'
  echo -e '/d\tShow differences in decimal format
/a\tShow differences as characters
/l\tShow the numbe
r of the line where a difference occurs, instead of displaying the byte offset
/n=\tCompare only the number of lines that are specified for each file, even if the files are different sizes
/c\tPerform a comparison that is not case-sensitive
/off\tProcess files with the offline attribute set
/offline\tProcess files with the offline attribute set
/?\tShow help'
end

complete --command comp --no-files --arguments '(__comp_generate_args)'
