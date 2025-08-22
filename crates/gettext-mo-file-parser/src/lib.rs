use std::collections::HashMap;

const U32_SIZE: usize = std::mem::size_of::<u32>();

fn read_le_u32(bytes: &[u8]) -> u32 {
    u32::from_le_bytes(bytes[..U32_SIZE].try_into().unwrap())
}

fn read_be_u32(bytes: &[u8]) -> u32 {
    u32::from_be_bytes(bytes[..U32_SIZE].try_into().unwrap())
}

fn get_u32_reader_from_magic_number(magic_number: &[u8]) -> std::io::Result<fn(&[u8]) -> u32> {
    match magic_number {
        [0x95, 0x04, 0x12, 0xde] => Ok(read_be_u32),
        [0xde, 0x12, 0x04, 0x95] => Ok(read_le_u32),
        _ => Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "First 4 bytes of MO file must correspond to magic number 0x950412de, either big or little endian.",
        )),
    }
}

/// Returns an error if an unknown major revision is detected.
/// There are no relevant differences between supported revisions.
fn check_if_revision_is_supported(revision: u32) -> std::io::Result<()> {
    // From the reference:
    // A program seeing an unexpected major revision number should stop reading the MO file entirely;
    // whereas an unexpected minor revision number means that the file can be read
    // but will not reveal its full contents,
    // when parsed by a program that supports only smaller minor revision numbers.
    let major_revision = revision >> 16;
    match major_revision {
        0 | 1 => {
            // At time of writing, these are the only major revisions which exist.
            // There is no documented difference and the GNU gettext code does not seem to
            // differentiate between the two either.
            // All features we care about are supported in minor revision 0,
            // so we do not need to care about the minor revision.
            Ok(())
        }
        _ => Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "Major revision must be 0 or 1",
        )),
    }
}

fn as_usize(value: u32) -> usize {
    use std::mem::size_of;
    const _: () = assert!(size_of::<u32>() <= size_of::<usize>());
    usize::try_from(value).unwrap()
}

fn parse_strings(
    file_content: &[u8],
    num_strings: usize,
    table_offset: usize,
    read_u32: fn(&[u8]) -> u32,
) -> std::io::Result<Vec<&[u8]>> {
    let file_too_short_error = || {
        Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "MO file is too short.",
        ))
    };
    if table_offset + num_strings * 2 * U32_SIZE > file_content.len() {
        return file_too_short_error();
    }
    let mut strings = Vec::with_capacity(num_strings);
    let mut offset = table_offset;
    let mut get_next_u32 = || {
        let val = read_u32(&file_content[offset..]);
        offset += U32_SIZE;
        val
    };
    for _ in 0..num_strings {
        // not including NUL terminator
        let string_length = as_usize(get_next_u32());
        let string_offset = as_usize(get_next_u32());
        let string_end = string_offset.checked_add(string_length).unwrap();
        if string_end > file_content.len() {
            return file_too_short_error();
        }
        // Contexts are stored by storing the concatenation of the context, a EOT byte, and the original string, instead of the original string.
        // Contexts are not supported by this implementation.
        // The format allows plural forms to appear behind singular forms, separated by a NUL byte,
        // where `string_length` includes the length of both.
        // This is not supported here.
        // Do not include the NUL terminator in the slice.
        strings.push(&file_content[string_offset..string_end]);
    }
    Ok(strings)
}

/// Parse a MO file.
/// Format reference used: <https://www.gnu.org/software/gettext/manual/html_node/MO-Files.html>
pub fn parse_mo_file(file_content: &[u8]) -> std::io::Result<HashMap<&[u8], &[u8]>> {
    if file_content.len() < 7 * U32_SIZE {
        return Err(std::io::Error::new(
            std::io::ErrorKind::InvalidData,
            "File too short to contain header.",
        ));
    }
    // The first 4 bytes are a magic number, from which the endianness can be determined.
    let read_u32 = get_u32_reader_from_magic_number(&file_content[0..U32_SIZE])?;
    let mut offset = U32_SIZE;
    let mut get_next_u32 = || {
        let val = read_u32(&file_content[offset..]);
        offset += U32_SIZE;
        val
    };
    let file_format_revision = get_next_u32();
    check_if_revision_is_supported(file_format_revision)?;
    let num_strings = as_usize(get_next_u32());
    let original_strings_offset = as_usize(get_next_u32());
    let translation_strings_offset = as_usize(get_next_u32());
    let original_strings =
        parse_strings(file_content, num_strings, original_strings_offset, read_u32)?;
    let translated_strings = parse_strings(
        file_content,
        num_strings,
        translation_strings_offset,
        read_u32,
    )?;
    let mut translation_map = HashMap::with_capacity(num_strings);
    for i in 0..num_strings {
        translation_map.insert(original_strings[i], translated_strings[i]);
    }
    Ok(translation_map)
}
