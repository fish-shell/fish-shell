if test (uname) = Darwin # OS X
    complete -c open -s a -d 'specify app name' -r -a "(mdfind -onlyin /Applications -onlyin ~/Applications -onlyin /System/Applications -onlyin /Developer/Applications 'kMDItemContentType==com.apple.application-*' | string replace -r '.+/(.+).app' '\$1')"
    complete -c open -s b -d 'specify app bundle id' -x -a "(mdls (mdfind -onlyin /Applications -onlyin ~/Applications -onlyin /System/Applications -onlyin /Developer/Applications 'kMDItemContentType==com.apple.application-*') -name kMDItemCFBundleIdentifier | string replace -rf 'kMDItemCFBundleIdentifier = \"(.+)\"' '\$1')"
    complete -c open -s e -d 'open in TextEdit'
    complete -c open -s t -d 'open in default editor'
    complete -c open -s f -d 'open stdin with editor'
    complete -c open -s F -d 'open app with fresh state'
    complete -c open -s W -d 'wait for app to exit'
    complete -c open -s R -d 'reveal in Finder'
    complete -c open -s n -d 'open new instance of app'
    complete -c open -s g -d 'open app in background'
    complete -c open -s h -d 'open library header file' -r
    complete -c open -l args -d 'pass remaining args to app' -x
end
