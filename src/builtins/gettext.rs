use super::prelude::*;

/// Used for the fish `_` builtin for requesting translations.
/// For scripts in `share/`, the corresponding strings are extracted from the scripts using
/// `cargo xtask gettext update`.
/// Strings not present in our repo would require a custom MO file for translation to be possible.
pub fn gettext(_parser: &mut Parser, streams: &mut IoStreams, argv: &mut [&wstr]) -> BuiltinResult {
    for arg in &argv[1..] {
        streams.out.append(
            crate::localization::LocalizableString::from_external_source((*arg).to_owned())
                .localize(),
        );
    }
    Ok(SUCCESS)
}
