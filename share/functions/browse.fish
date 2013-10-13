function browse --description 'Browse a website or local HTML documentation.'

  if [ (protocol $argv) ]
    set URL $argv
  else
    if [ -e (pwd)/(dirname $argv)/(basename $argv) ]
      set URL file://(pwd)/(dirname $argv)/(basename $argv)
    else
      if [ -e (dirname $argv)/(basename $argv) ]
        set URL file://(dirname $argv)/(basename $argv)
      else
        set URL http://$argv
      end
    end
  end

  if not set -q BROWSER
    if [ (uname) = Darwin ]

      # Uses the Automator action 'Website Popup' to display the given
      # URI without opening a seperate application. The workflow itself
      # is some real funky XML, so it's gzip'd and base64'd. We use
      # openssl for base64 decoding since the base64 utility doesn't
      # exist prior to 10.7, and early Python versions may not have it.

      automator -D URI="$URL" (echo '
      H4sICHBVWlICA3dmbG93LndmbG93AKVWbW/aMBD+zq/gD5g4jpPYE6pkx8lUad2q
      vm2TKlUudVk2iCPHVGO/fiYECIZNpeUDSnJ3z91z58f2uJ6VjT0bP5UT9/9LLc/k
      xJa6asbB6mUsjZHLQ3NnbT8P2u/s4lIaOVdWmUuja2VsqZq+W+v1Ik0pH2fq9vZc
      HGI0aqYmVj3dHXgNxo01ZTU9ixEXsRAcYJFCgLMQAYIYA6IQtAiLAoWYjYPOeRys
      Mwf9Otv6+aJ6mqlLaX94+MH1srFqHnwqH400y4AtrJ5Lq03wUdnhnZwt1FA/DzcV
      jjbt6ML7Obb9eE0b3kVwnXXN6fxJVbZ8LpXxgCd6PpJ17Wrecho5Tg8tpwf9/LDh
      5JHJZrJphp8dlY5GZ31N7HlVL2xv1p2RQEcC5TFIaSwA5igCLMMpiKIi5RFCHLHC
      Q/qysEehWIKwaw8EhCU5wCjNAGdJBEJMIREFoXnCPahDEJjkUSZIBPIoR67xCQKU
      FBwkOWSY0ixMSeSBzPRE9oTQ2SJIRzFc/T5gko7aJ+hFVuXjbt2duOzuB97CCzJd
      WTfwJrhSjV6YiWoCLhs1mtVG/wzmsqxGLmGvhD1J/FfX75PLV/XYlFYNL3W9qN8o
      E90O/WZZqxs57ayl4ztV5gyNg83jGq3WLp/DO3QNd67rbab8owptXJ1b38HWmVLP
      20ozVfbaxXzzcMlqtvtF7Jy/e85pQjzgRaMMm7rhHamiB7wZ1Rsl3s3hoZ3DbuM4
      puwtDrvoolyQi2L7wzum7G1oiHJOBY8BD9MYYMoxYIQiUBDCQ5jBLOV0H8lX9q4K
      EYYwEhnIClEATJwqCS8g4DBlLC1YImIP6lg9bj8hJMudtDEFGJIYEJohEGcRjSOY
      E8ijfZB9ZQ+OSDsi2Jf2obJPkMj9oC+SUyXtH3JBd2SvipjoqnInqt7X1nqpFmGe
      poznIMbQtZcLCmjmGp3mOU9DnidRIQ4U+Wz03OP3ms1zCBzJk3bZtZr0G6bZ5jpt
      7EeuCZsT+p/3oHcf3avQ8hQlb7f+G/XbeivvUMO3V+f7Pi+rQ6TPJtgR79htXtd3
      wr8PX5ASGwoAAA=='|openssl base64 -d|gzcat|psub) 2>&1 >/dev/null&
    end
  else
    if [ (uname) = Darwin ]
      switch $BROWSER
        case Aurora aurora
          set BundleIdentifier org.mozilla.aurora
        case Camino camino
          set BundleIdentifier org.mozilla.camino
        case Canary canary 'Google Chrome Canary'
          set BundleIdentifier com.google.Chrome.canary
        case Chrome chrome 'Google Chrome'
          set BundleIdentifier com.google.chrome
        case Chromium chromium
          set BundleIdentifier org.chromium.Chromium
        case Firefox firefox
          set BundleIdentifier org.mozilla.firefox
        case Maxthon maxthon
          set BundleIdentifier com.maxthon.Maxthon
        case Netscape netscape Navigator navigator 'Netscape Navigator'
          set BundleIdentifier com.netscape.navigator
        case Next next 'Opera Next'
          set BundleIdentifier com.operasoftware.OperaNext
        case Opera opera
          set BundleIdentifier com.operasoftware.opera
        case Safari safari
          set BundleIdentifier com.apple.safari
        case WebKit Webkit webkit
          set BundleIdentifier org.webkit.nightly.WebKit
      end
  
      # I thought Chrome couldn't handle file:// URIs? But they seem to be
      # working for me. Uncomment below if I'm mistaken.

      # if [ (protocol $URL) = file ]
      #   switch $BundleIdentifier
      #     case 'com.google*' 'org.chromium*'
      #       set BundleIdentifier com.apple.safari
      #   end
      # end

      if set -q BundleIdentifier
        open -b $BundleIdentifier "$URL"
      else
        eval $BROWSER "'$URL'"
      end
    end
  end
end
