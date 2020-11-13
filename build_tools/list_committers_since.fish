#! /usr/bin/env fish

set -l TAG $argv[1]

if test -z "$TAG"
    echo "Tag name required."
    exit 1
end

if not contains -- $TAG (git tag)
    echo "$TAG is not a valid tag name."
    exit 1
end

set -l committers_to_tag (mktemp)
or exit 1
set -l committers_from_tag (mktemp)
or exit 1

# You might think it would be better to case-insensitively sort/compare the names
# to produce a more natural-looking list.
# Unicode collation tables mean that this is fraught with danger; for example, the
# "“" character will not case-fold in UTF-8 locales. sort suggests using the C locale!

git log "$TAG" --format="%aN" --reverse | sort -u >$committers_to_tag
git log "$TAG".. --format="%aN" --reverse | sort -u >$committers_from_tag

echo New committers:
echo (comm -13 $committers_to_tag $committers_from_tag)','

echo

echo Returning committers:
echo (comm -12 $committers_to_tag $committers_from_tag)','

rm $committers_to_tag $committers_from_tag
