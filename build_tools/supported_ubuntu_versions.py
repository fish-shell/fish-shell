# /// script
# requires-python = ">=3.5"
# dependencies = [
#     "launchpadlib",
# ]
# ///

from launchpadlib.launchpad import Launchpad

if __name__ == "__main__":
    launchpad = Launchpad.login_anonymously(
        "fish shell build script", "production", "~/.cache", version="devel"
    )
    ubu = launchpad.projects("ubuntu")
    print(
        "\n".join(
            x["name"]
            for x in ubu.series.entries
            if x["supported"] == True
            and x["name"] not in ("trusty", "xenial", "bionic", "focal")
        )
    )
