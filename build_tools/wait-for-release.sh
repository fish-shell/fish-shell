#!/bin/sh

# Wait until the given release is public.

set -eux

tag=$1
tag_oid=$2
if [ $# -eq 2 ]; then
    repository_owner=fish-shell
    remote=origin
else
    repository_owner=$3
    remote=$4
    [ $# -eq 4 ]
fi

while ! {
    repo_api_url=https://api.github.com/repos/$repository_owner/fish-shell
    curl -sL \
        -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" \
        "$repo_api_url/releases/tags/$tag" |
        python -c "$(cat <<-EOF
			import json, sys
			release = json.load(sys.stdin)
			if release.get("draft", None) == False:
			    sys.exit(0)
			sys.exit(1)
			EOF
        )"
    }
do
    sleep 30s
done

actual_tag_oid=$(git ls-remote $remote | awk '$2 == "refs/tags/'"$tag"'" { print $1 }')
[ "$tag_oid" = "$actual_tag_oid" ]
