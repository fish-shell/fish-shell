from datetime import datetime
from pathlib import Path
from typing import Tuple
import dateutil.tz
import json
import re
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

# TODO maybe use https://github.com/getpelican/feedgenerator instead?
from feedgen.feed import FeedGenerator
import git
import polib


def message_dictionary(repo, ref, file: Path):
    po_contents = repo.git().show(f"{ref}:{file}")
    messages = polib.pofile(po_contents)
    return {e.msgid: e.msgstr for e in messages}


def write_feed(
    feed_path: Path,
    base_ref_oid: str,
    title: str,
    subtitle: str,
    this_pr: Tuple[str, str] | None,
):
    fg = FeedGenerator()
    tree_url = (
        "https://raw.githubusercontent.com/fish-shell/fish-shell/fish-shell-events"
    )
    feed_url = f"{tree_url}/{feed_path}"
    fg.id(feed_url)
    fg.title(title)
    fg.subtitle(subtitle)
    fg.link(href=feed_url, rel="self")
    generator_url = f"https://github.com/fish-shell/fish-shell/blob/{base_ref_oid}/.github/workflows/update-event-feeds.yml"
    fg.link(href=generator_url, rel="alternate")

    entries = {}
    if feed_path.exists():
        tree = ET.parse(feed_path)
        root = tree.getroot()
        xmlns = "{http://www.w3.org/2005/Atom}"
        for child in root.findall(f"{xmlns}entry"):
            pr_url = child.find(f"{xmlns}id").text
            pr_title = child.find(f"{xmlns}title").text
            updated = child.find(f"{xmlns}updated").text
            updated = datetime.fromisoformat(updated)
            entries[pr_url] = (pr_title, updated)
    if this_pr is not None:
        (this_pr_title, this_pr_url) = this_pr
        if this_pr_url not in entries:
            entries[this_pr_url] = (this_pr_title, datetime.now(dateutil.tz.tzutc()))
    for pr_url, (pr_title, updated) in entries.items():
        fe = fg.add_entry()
        fe.id(pr_url)
        fe.title(pr_title)
        fe.updated(updated)
        fe.link(href=pr_url)
    atomfeed = fg.atom_str(pretty=True).decode()
    feed_path.write_text(atomfeed)


def pull_request_data(pr_number: str):
    data = json.loads(
        subprocess.check_output(
            (
                "gh",
                "pr",
                "view",
                "--json",
                "baseRefOid,headRefOid,title,url",
                str(pr_number),
            ),
            text=True,
        )
    )
    base_ref_oid = data["baseRefOid"]
    head_ref_oid = data["headRefOid"]
    this_pr_title = data["title"]
    this_pr_url = data["url"]
    return (base_ref_oid, head_ref_oid, this_pr_title, this_pr_url)


def main():
    pr_number = sys.argv[1]

    (base_ref_oid, head_ref_oid, this_pr_title, this_pr_url) = pull_request_data(
        pr_number
    )

    repo = git.Repo()
    repo.git().execute(("git", "fetch", "origin", base_ref_oid, head_ref_oid))
    po_files = (
        Path(file)
        for file in repo.git()
        .execute(("git", "ls-tree", "--name-only", base_ref_oid, ":/po/"))
        .splitlines()
    )
    changed_files = {
        Path(file)
        for file in subprocess.check_output(
            ("git", "diff", "--name-only", base_ref_oid, head_ref_oid), text=True
        ).splitlines()
        if re.match(r"^po/\w+\.po$", file)
    }
    Path("po").mkdir(exist_ok=True)
    for po_file in po_files:
        file_without_extension = str(po_file).removesuffix(".po")
        ll_CC = os.path.basename(file_without_extension)
        feed_additions_path = Path(f"{file_without_extension}-additions.atom")
        feed_removals_path = Path(f"{file_without_extension}-removals.atom")

        def title(verb: str):
            return f"fish-shell PRs {verb} {ll_CC} translations"

        def write_additions(this_pr):
            subtitle = "PRs that add new translations"
            write_feed(
                feed_additions_path, base_ref_oid, title("adding"), subtitle, this_pr
            )

        def write_removals(this_pr):
            subtitle = "PRs that remove translations, often due to a change in the messages source"
            write_feed(
                feed_removals_path, base_ref_oid, title("removing"), subtitle, this_pr
            )

        if not feed_additions_path.exists():
            write_additions(None)
        if not feed_removals_path.exists():
            write_removals(None)

        if po_file not in changed_files:
            continue
        old = message_dictionary(repo, base_ref_oid, po_file)
        new = message_dictionary(repo, head_ref_oid, po_file)
        if any(
            new_msgstr != old.get(new_msgid) and new_msgstr != ""
            for (new_msgid, new_msgstr) in new.items()
        ):
            write_additions((this_pr_title, this_pr_url))
        elif any(
            old_msgstr != "" and new.get(old_msgid, "") == ""
            for (old_msgid, old_msgstr) in old.items()
        ):
            write_removals((this_pr_title, this_pr_url))


if __name__ == "__main__":
    main()
