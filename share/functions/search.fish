function search --description 'Search the web.'
  set terms (echo $argv | sed -e 's/\ /\+/g')
  if not set -q SEARCHENGINE; set SEARCHENGINE google; end

  switch $SEARCHENGINE
    case Google google google.com
      set ENGINE http://google.com/search
    case Bing bing bing.com
      set ENGINE http://bing.com/search
    case Yahoo yahoo yahoo.com
      set ENGINE http://search.yahoo.com/search
    case DuckDuckGo duckduckgo duckduckgo.com
      set ENGINE http://duckduckgo.com/
    case '*'
      set ENGINE $SEARCHENGINE
  end 
  browse "$ENGINE/?q=$terms"
end
