# codesign - create and manipulate code signatures (man 1 codesign)
#
# codesign takes GNU-style long options (--name / --name=value) and classic
# single-character options. Exactly one operation option (-s/-v/-d/-h/
# --validate-constraint) selects the action; the rest modify behaviour.
# Operands are paths (files/bundles) or, for verify/display/hosting, pids.

# ── live enumerator: codesigning identities ──────────────────────────
# `security find-identity -v -p codesigning` is fast and unprivileged. Lines
# look like:  1) <hash> "Apple Development: name (TEAMID)" — extract the
# quoted common-name and offer it as the -s/--sign value.
function __fish_codesign_identities
    security find-identity -v -p codesigning 2>/dev/null \
        | string match -r '"[^"]+"' \
        | string trim -c '"'
end

# ── operation options (one required) ─────────────────────────────────
complete -c codesign -s s -l sign -x -a '(__fish_codesign_identities)' \
    -d 'Sign the code at the path(s) using this identity'
complete -c codesign -s v -l verify -d 'Verify code signatures on the path(s)/pid(s)'
complete -c codesign -s d -l display -d 'Display information about the code at the path(s)/pid(s)'
complete -c codesign -s h -l hosting -d 'Construct and print the hosting chain of running pid(s)'
complete -c codesign -l validate-constraint -d 'Validate that constraint plist(s) are structurally valid'

# ── signing options ──────────────────────────────────────────────────
complete -c codesign -s f -l force -d 'Replace any existing signature on the path(s)'
complete -c codesign -s i -l identifier -x -d 'Explicitly specify the unique identifier embedded in the signature'
complete -c codesign -l prefix -x -d 'Prefix string for an implicitly generated identifier'
complete -c codesign -s o -l options -x \
    -a 'kill hard host expires library runtime linker-signed' \
    -d 'Comma-separated code-signature option flags to embed'
complete -c codesign -s r -l requirements -r -d 'Internal requirements to embed (file, =text, or - for stdin)'
complete -c codesign -l keychain -r -d 'Search for the signing identity only in this keychain file'
complete -c codesign -l entitlements -r -d 'Embed (signing) or extract (display) entitlement data'
complete -c codesign -l force-library-entitlements -d 'Forcefully embed entitlements in library signatures'
complete -c codesign -l generate-entitlement-der -d 'Embed entitlements as both XML and DER'
complete -c codesign -l preserve-metadata -x \
    -a 'identifier entitlements requirements flags runtime launch-constraints library-constraints' \
    -d 'When re-signing, reuse some information from the old signature'
complete -c codesign -s D -l detached -r -d 'Write (or read) a detached signature to/from this file'
complete -c codesign -l detached-database -d 'Write the detached signature into the system database'
complete -c codesign -s P -l pagesize -x -d 'Granularity of code signing (a power of two)'
complete -c codesign -l timestamp -x -d 'Contact a timestamp authority (=URL, or none to disable)'
complete -c codesign -l runtime-version -x -d 'Hardened runtime version when the runtime flag is set'
complete -c codesign -l dryrun -d 'Perform signing operations without writing the result'
complete -c codesign -l strip-disallowed-xattrs -d 'Strip xattrs that could interfere with code signing'
complete -c codesign -l single-threaded-signing -d 'Use one thread for building the resource seal'
complete -c codesign -l launch-constraint-self -r -d 'Embed a launch constraint plist on this executable'
complete -c codesign -l launch-constraint-parent -r -d "Embed a launch constraint plist on the executable's parent"
complete -c codesign -l launch-constraint-responsible -r -d 'Embed a launch constraint plist on the responsible process'
complete -c codesign -l library-constraint -r -d 'Embed a constraint plist on libraries this process can load'
complete -c codesign -l enforce-constraint-validity -d 'Require that supplied constraints are structurally valid'
complete -c codesign -l remove-signature -d 'Remove the current code signature from the path(s)'

# ── verification / display options ───────────────────────────────────
complete -c codesign -s R -l test-requirement -r -d 'Verify the path(s) against this code requirement'
complete -c codesign -l deep -d 'Recursively sign/verify/display nested code content'
complete -c codesign -s a -l architecture -x -d 'Select the Mach-O architecture to verify or display'
complete -c codesign -l all-architectures -d 'Verify each architecture in a universal Mach-O separately'
complete -c codesign -l bundle-version -x -d 'Versioned-bundle version to operate on'
complete -c codesign -l check-notarization -d 'Force an online notarization check during verification'
complete -c codesign -l strict -f -a 'all symlinks sideband' -d 'Apply additional validation restrictions'
complete -c codesign -l ignore-resources -d "Do not validate the code's resources during static validation"
complete -c codesign -l extract-certificates -r -d 'Extract the embedded certificate chain to files (prefix)'
complete -c codesign -l file-list -r -d 'Write the list of possibly-modified files to this path (- for stdout)'
complete -c codesign -l xml -d 'Output extracted entitlements as XML'
complete -c codesign -l der -d 'Output extracted entitlements as DER'

# ── general ──────────────────────────────────────────────────────────
complete -c codesign -l continue -d 'Continue processing path arguments even if one fails'
complete -c codesign -l verbose -d 'Set or increment the verbosity level of output'
