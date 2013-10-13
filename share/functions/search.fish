function search --description 'Search the web.'
  set terms (echo $argv | sed -e 's/\ /\+/g')
  if not set -q SEARCHENGINE; set SEARCHENGINE google; end

  switch $SEARCHENGINE
    case Google google google.com
      set ENGINE http://google.com
    case Bing bing bing.com
      set ENGINE http://bing.com
    case Yahoo yahoo yahoo.com
      set ENGINE http://search.yahoo.com
  end 

  browse "$ENGINE/search?q=$terms"
end