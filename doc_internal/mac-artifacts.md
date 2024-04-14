# Building macOS Artifacts

This describes how to build macOS artifacts as part of a release. These artifacts are uploaded to the GitHub release page, where they are discovered by the web site build script.

Artifacts may be built locally or in CI. Using CI is preferred.

> **Note**
> Only fish-shell administrations may create releases. Released macOS packages require code signing and notarization via private Apple developer keys, which are owned by @ridiculous_fish. These keys are stored in GitHub secrets.

## Building in CI (GitHub Actions)

macOS packages may be built in CI through a GitHub workflow. This requires a fish-shell administrator as it requires invoking secret code signing keys.

Steps:

1. Go to https://github.com/fish-shell/fish-shell/actions
2. On the left side, click on the "macOS build and codesign" Action.
3. On the right, click on the "Run workflow" button, select the branch or tag, and click Run Workflow.
4. **Reload the page**. This is necessary because the workflow will not appear until the page is reloaded.
5. Click on the new workflow. It should be in a Waiting state.
6. Click on Review Deployments, and approve and start the "deployment."
7. Once the workflow completes, expand the `actions/upload-artifact@v4` step in the logs. This should have an "Artifact download URL" - click it and download!

## Building locally (no code signing)

To build locally without notarizing and code signing, use the `build_tools/make_macos_pkg.sh` script:

```
> ./build_tools/make_macos_pkg.sh
```

Packages will be placed in `~/fish_built` by default.

Note these packages will result in loud warnings or errors when others try to install them, because of the lack of code signing.

## Building locally with code signing and notarization

You will need the following:

-   The ".p12" certificate files for both "Developer ID Application" and "Developer ID Installer".
    -   (These can be generated via Export Certificate in Xcode)
-   The password for these files (by convention, the same password is used for both).
-   The JSON file generated via `rcodesign encode-app-store-connect-api-key`.

An example run:

```
> ./build_tools/make_macos_pkg.sh -s \
    -f fish-developer-id-application.p12 \
    -i fish-developer-id-installer.p12 \
    -p "$NOTARIZE_PASSWORD"  \
    -n \
    -j notarize-data.json
```
