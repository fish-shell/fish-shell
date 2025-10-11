#RUN: %fish %s

#REQUIRES: command -v tmux
#REQUIRES: command -v wget
#REQUIRES: command -v diff

isolated-tmux-start

# Widen the window because URLs can be fairly long and we do not want to deal
# with wrapping.
# We would prefer to redirect fish_config's output to a file instead and avoid
# the issue altogether, but we need to extract the auth string and unfortunately,
# because of buffering, the file will remain empty until the buffer is full or
# fish_config exits.
isolated-tmux resize-window -x 500

isolated-tmux send-keys "BROWSER=true fish_config" Enter
sleep-until 'isolated-tmux capture-pane -p | grep ENTER'
isolated-tmux capture-pane -p
# CHECK: prompt 0> BROWSER=true fish_config
# CHECK: Web config started at file://{{.*}}.html
# CHECK: If that doesn't work, try opening http://localhost:{{(\d{4})}}/{{\w+}}/
# CHECK: Hit ENTER to stop.

# Extract the URL from the output
set -l base_url (isolated-tmux capture-pane -p | string match -r 'http://localhost:\d{4}/\w+/$')
or exit
set -l host_port (dirname $base_url)

# Check a bad URL (http://host:port/invalid_auth/)
wget -q -O /dev/null $host_port/invalid_auth/ 2>/dev/null
set -l last_status $status
# Busybox's wget does not return the same code as GNU's. Currently this affects
# the Alpine CI. If the Alpine image is update to GNU wget, we should be able to
# safely assume everybody uses GNU's (until we told otherwise)
switch "$(wget --version 2>&1)"
    case '*GNU*'
        test $last_status -eq 8
    case '*busybox*'
        test $last_status -eq 1
    case '*'
        # Only rely on the fish_config logs, which is the critical test.
        # The status code is only a "nice to have"
        true
end
or echo "Unexpected exit code ($last_status) from wget"

isolated-tmux capture-pane -p -S 4
# CHECK: {{.*}} code 403, message Forbidden, path /invalid_auth/

# Check a good URL
set -l workspace_root (path resolve -- (status dirname)/../../)
diff -q "$workspace_root/share/tools/web_config/index.html" (wget -q -O - $base_url 2>/dev/null | psub -f)

isolated-tmux send-keys Enter
tmux-sleep
isolated-tmux capture-pane -p -S 5
# CHECK: Shutting down.
# CHECK: prompt 1>
