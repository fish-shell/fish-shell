use crate::common::{
    UnescapeFlags, UnescapeStringStyle, unescape_string, valid_var_name, wcs2zstring,
};
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::flog::{flog, flogf};
use crate::fs::{PotentialUpdate, lock_and_load, rewrite_via_temporary_file};
use crate::path::path_get_config;
use crate::wchar::{decode_byte_from_char, prelude::*};
use crate::wcstringutil::{LineIterator, join_strings};
use crate::wutil::{FileId, INVALID_FILE_ID, file_id_for_file, file_id_for_path_narrow, wrealpath};
use std::collections::HashSet;
use std::collections::hash_map::Entry;
use std::ffi::CString;
use std::fs::File;
use std::io::{Read, Write};
use std::mem::MaybeUninit;

/// Callback data, reflecting a change in universal variables.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CallbackData {
    // The name of the variable.
    pub key: WString,

    // The value of the variable, or none if it is erased.
    pub val: Option<EnvVar>,
}

pub type CallbackDataList = Vec<CallbackData>;

// List of fish universal variable formats.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
enum UvarFormat {
    Fish_2_x,
    Fish_3_0,
    Future,
}

/// Class representing universal variables.
pub struct EnvUniversal {
    // Path that we save to. This is set in initialize(). If empty, initialize has not been called.
    vars_path: WString,
    narrow_vars_path: CString,

    // The table of variables.
    vars: VarTable,

    // Keys that have been modified, and need to be written. A value here that is not present in
    // vars indicates a deleted value.
    modified: HashSet<WString>,

    // A generation count which is incremented every time an exported variable is modified.
    export_generation: u64,

    // Whether it's OK to save. This may be set to false if we discover that a future version of
    // fish wrote the uvars contents.
    // Only update if last_read_file_id is updated as well.
    ok_to_save: bool,

    // File id from which we last read.
    // Only update if ok_to_save is updated as well.
    last_read_file_id: FileId,
}

struct UniversalReadUpdate {
    export_generation_increment: u64,
    new_vars: VarTable,
    callbacks: CallbackDataList,
    ok_to_save: bool,
}

impl EnvUniversal {
    // Construct an empty universal variables.
    pub fn new() -> Self {
        Self {
            vars_path: Default::default(),
            narrow_vars_path: Default::default(),
            vars: Default::default(),
            modified: Default::default(),
            export_generation: 1,
            ok_to_save: true,
            last_read_file_id: INVALID_FILE_ID,
        }
    }
    // Get the value of the variable with the specified name.
    pub fn get(&self, name: &wstr) -> Option<EnvVar> {
        self.vars.get(name).cloned()
    }
    // Return flags from the variable with the given name.
    pub fn get_flags(&self, name: &wstr) -> Option<EnvVarFlags> {
        self.vars.get(name).map(|var| var.get_flags())
    }
    // Sets a variable.
    pub fn set(&mut self, key: &wstr, var: EnvVar) {
        let exports = var.exports();
        match self.vars.entry(key.to_owned()) {
            Entry::Occupied(mut entry) => {
                if entry.get() == &var {
                    return;
                }
                entry.insert(var);
            }
            Entry::Vacant(entry) => {
                entry.insert(var);
            }
        };
        self.modified.insert(key.to_owned());
        if exports {
            self.export_generation += 1;
        }
    }
    // Removes a variable. Returns true if it was found, false if not.
    pub fn remove(&mut self, key: &wstr) -> bool {
        if let Some(var) = self.vars.remove(key) {
            if var.exports() {
                self.export_generation += 1;
            }
            self.modified.insert(key.to_owned());
            return true;
        }
        false
    }

    // Gets variable names.
    pub fn get_names(&self, show_exported: bool, show_unexported: bool) -> Vec<WString> {
        let mut result = vec![];
        for (key, var) in &self.vars {
            if (var.exports() && show_exported) || (!var.exports() && show_unexported) {
                result.push(key.clone());
            }
        }
        result
    }

    /// Get a view on the universal variable table.
    pub fn get_table(&self) -> &VarTable {
        &self.vars
    }

    /// Initialize this uvars for the default path.
    /// This should be called at most once on any given instance.
    pub fn initialize(&mut self) -> Option<CallbackDataList> {
        self.initialize_at_path(default_vars_path())
    }

    /// Initialize a this uvars for a given path.
    fn initialize_at_path(&mut self, path: WString) -> Option<CallbackDataList> {
        if path.is_empty() {
            return None;
        }
        assert!(!self.initialized(), "Already initialized");
        self.vars_path = path;

        self.load_from_path()
    }

    /// Reads and writes variables at the correct path.
    /// If there is an existing variables file with an unknown format, parsing it is attempted,
    /// but it will not be overwritten.
    /// Returns whether data was read, and the callbacks.
    pub fn sync(&mut self) -> (bool, Option<CallbackDataList>) {
        if !self.initialized() {
            return (false, None);
        }

        flog!(uvar_file, "universal log sync");
        // If we have no changes, just load.
        if self.modified.is_empty() {
            let callbacks = self.load_from_path_narrow();
            flog!(uvar_file, "universal log no modifications");
            return (false, callbacks);
        }

        flog!(uvar_file, "universal log performing full sync");

        let rewrite = |old_file: &File,
                       tmp_file: &mut File|
         -> std::io::Result<PotentialUpdate<Option<UniversalReadUpdate>>> {
            match self.load_from_file(old_file, file_id_for_file(old_file)) {
                Some(potential_update) => {
                    if potential_update.do_save {
                        let contents = Self::serialize_with_vars(&potential_update.data.new_vars);
                        tmp_file.write_all(&contents)?;
                    }
                    Ok(PotentialUpdate {
                        do_save: potential_update.do_save,
                        data: Some(potential_update.data),
                    })
                }
                None => {
                    if self.ok_to_save {
                        let contents = Self::serialize_with_vars(&self.vars);
                        tmp_file.write_all(&contents)?;
                    }
                    Ok(PotentialUpdate {
                        do_save: self.ok_to_save,
                        data: None,
                    })
                }
            }
        };

        let real_path = wrealpath(&self.vars_path).unwrap_or_else(|| self.vars_path.clone());
        match rewrite_via_temporary_file(&real_path, rewrite) {
            Ok((file_id, potential_update)) => {
                self.last_read_file_id = file_id;
                self.ok_to_save = potential_update.do_save;
                self.modified.clear();
                match potential_update.data {
                    Some(UniversalReadUpdate {
                        export_generation_increment,
                        new_vars,
                        callbacks,
                        ok_to_save,
                    }) => {
                        assert_eq!(potential_update.do_save, ok_to_save);
                        self.export_generation += export_generation_increment;
                        self.vars = new_vars;
                        (true, Some(callbacks))
                    }
                    None => (true, None),
                }
            }
            Err(e) => {
                flog!(uvar_file, "universal log sync failed:", e);
                (false, None)
            }
        }
    }

    /// Populate a variable table `out_vars` from a `s` string.
    /// Return the format of the file that we read.
    fn populate_variables(s: &[u8], out_vars: &mut VarTable) -> UvarFormat {
        // Decide on the format.
        let format = Self::format_for_contents(s);

        let iter = LineIterator::new(s);
        let mut wide_line = WString::new();
        let mut storage = WString::new();
        for line in iter {
            // Skip empties and constants.
            if line.is_empty() || line[0] == b'#' {
                continue;
            }

            // Convert to UTF8.
            wide_line.clear();
            let Ok(line) = std::str::from_utf8(line) else {
                continue;
            };
            wide_line = WString::from_str(line);

            match format {
                UvarFormat::Fish_2_x => {
                    Self::parse_message_2x_internal(&wide_line, out_vars, &mut storage);
                }
                UvarFormat::Fish_3_0 => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
                // For future formats, just try with the most recent one.
                UvarFormat::Future => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
            }
        }
        format
    }

    /// Guess a file format.
    /// Return the format corresponding to file contents `s`.
    fn format_for_contents(s: &[u8]) -> UvarFormat {
        // Walk over leading comments, looking for one like '# version'
        let iter = LineIterator::new(s);
        for line in iter {
            if line.is_empty() {
                continue;
            }
            if line[0] != b'#' {
                // Exhausted leading comments.
                break;
            }
            // Note scanf %s is max characters to write; add 1 for null terminator.
            let mut versionbuf: MaybeUninit<[u8; 64 + 1]> = MaybeUninit::uninit();
            // Safety: test-only
            let cstr = CString::new(line).unwrap();
            if unsafe {
                libc::sscanf(
                    cstr.as_ptr(),
                    c"# VERSION: %64s".as_ptr().cast(),
                    versionbuf.as_mut_ptr(),
                )
            } != 1
            {
                continue;
            }

            // Try reading the version.
            let versionbuf = unsafe { versionbuf.assume_init() };
            return if versionbuf.starts_with(UVARS_VERSION_3_0)
                && versionbuf[UVARS_VERSION_3_0.len()] == b'\0'
            {
                UvarFormat::Fish_3_0
            } else {
                UvarFormat::Future
            };
        }
        // No version found, assume 2.x
        return UvarFormat::Fish_2_x;
    }

    /// Serialize a variable list.
    fn serialize_with_vars(vars: &VarTable) -> Vec<u8> {
        let mut contents = vec![];
        contents.extend_from_slice(SAVE_MSG);
        contents.extend_from_slice(b"# VERSION: ");
        contents.extend_from_slice(UVARS_VERSION_3_0);
        contents.push(b'\n');

        // Preserve legacy behavior by sorting the values first
        let mut cloned: Vec<(&wstr, &EnvVar)> =
            vars.iter().map(|(key, var)| (key.as_ref(), var)).collect();
        cloned.sort_by(|(lkey, _), (rkey, _)| lkey.cmp(rkey));

        for (key, var) in cloned {
            // Append the entry. Note that append_file_entry may fail, but that only affects one
            // variable; soldier on.
            append_file_entry(
                var.get_flags(),
                key,
                &encode_serialized(var.as_list()),
                &mut contents,
            );
        }
        contents
    }

    #[cfg(test)]
    pub fn is_ok_to_save(&self) -> bool {
        self.ok_to_save
    }

    /// Access the export generation.
    pub fn get_export_generation(&self) -> u64 {
        self.export_generation
    }

    /// Return whether we are initialized.
    fn initialized(&self) -> bool {
        !self.vars_path.is_empty()
    }

    fn load_from_path(&mut self) -> Option<CallbackDataList> {
        self.narrow_vars_path = wcs2zstring(&self.vars_path);
        self.load_from_path_narrow()
    }

    fn load_from_path_narrow(&mut self) -> Option<CallbackDataList> {
        // Check to see if the file is unchanged. We do this again in load_from_file, but this avoids
        // opening the file unnecessarily.
        if self.last_read_file_id != INVALID_FILE_ID
            && file_id_for_path_narrow(&self.narrow_vars_path) == self.last_read_file_id
        {
            flog!(uvar_file, "universal log sync elided based on fast stat()");
            return None;
        }

        flog!(uvar_file, "universal log reading from file");
        match lock_and_load(&self.vars_path, |f, file_id| {
            Ok(self.load_from_file(f, file_id).map(|update| update.data))
        }) {
            Ok((
                file_id,
                Some(UniversalReadUpdate {
                    export_generation_increment,
                    new_vars,
                    callbacks,
                    ok_to_save,
                }),
            )) => {
                self.export_generation += export_generation_increment;
                self.vars = new_vars;
                self.ok_to_save = ok_to_save;
                self.last_read_file_id = file_id;
                Some(callbacks)
            }
            Ok((_, None)) => {
                // WARNING: Do not update self.file_id here, as this would interfere with
                // self.ok_to_save.
                None
            }
            Err(e) => {
                flog!(uvar_file, "Failed to load from universal variable file:", e);
                None
            }
        }
    }

    /// Load environment variables from the [`file`](`File`).
    /// If the file has not been modified since the last read, [`None`] is returned.
    /// In this case, `self.ok_to_save` needs to be consulted to decide whether to rewrite the file.
    /// Otherwise, the [`Some`] variant contains updated data and indicates whether the file should
    /// be rewritten..
    // IMPORTANT: Callers of this code assume that a return value of None means that the file id has
    // not changed. Do not return None in other situations without modifying the callers
    // accordingly. Otherwise, problems with self.ok_to_save are expected to occur.
    fn load_from_file(
        &self,
        file: &File,
        current_file_id: FileId,
    ) -> Option<PotentialUpdate<UniversalReadUpdate>> {
        if current_file_id == self.last_read_file_id {
            flog!(uvar_file, "universal log sync elided based on fstat()");
            None
        } else {
            // Read a variables table from the file.
            let mut new_vars = VarTable::new();
            let format = Self::read_message_internal(file, &mut new_vars);

            // Hacky: if the read format is in the future, avoid overwriting the file: never try to
            // save.
            let do_save = format != UvarFormat::Future;

            // Announce changes and update our exports generation.
            let (export_generation_increment, callbacks) =
                self.generate_callbacks_and_update_exports(&new_vars);

            // Acquire the new variables.
            self.acquire_variables(&mut new_vars);
            Some(PotentialUpdate {
                do_save,
                data: UniversalReadUpdate {
                    export_generation_increment,
                    new_vars,
                    callbacks,
                    ok_to_save: do_save,
                },
            })
        }
    }

    /// Given a variable table, generate callbacks representing the difference between our vars and
    /// the new vars.
    /// Returns by how much the exports generation count should be incremented, as well as a
    /// callback list.
    fn generate_callbacks_and_update_exports(
        &self,
        new_vars: &VarTable,
    ) -> (u64, CallbackDataList) {
        let mut export_generation_increment = 0;
        let mut callbacks = CallbackDataList::new();
        // Construct callbacks for erased values.
        for (key, value) in &self.vars {
            // Skip modified values.
            if self.modified.contains(key) {
                continue;
            }

            // If the value is not present in new_vars, it has been erased.
            if !new_vars.contains_key(key) {
                callbacks.push(CallbackData {
                    key: key.clone(),
                    val: None,
                });
                if value.exports() {
                    export_generation_increment += 1;
                }
            }
        }

        // Construct callbacks for newly inserted or changed values.
        for (key, new_entry) in new_vars {
            // Skip modified values.
            if self.modified.contains(key) {
                continue;
            }

            let existing = self.vars.get(key);

            // See if the value has changed.
            let old_exports = existing.is_some_and(|v| v.exports());
            let export_changed = old_exports != new_entry.exports();
            let value_changed = existing.is_some_and(|v| v != new_entry);
            if export_changed || value_changed {
                export_generation_increment += 1;
            }
            if existing.is_none() || export_changed || value_changed {
                // Value is set for the first time, or has changed.
                callbacks.push(CallbackData {
                    key: key.clone(),
                    val: Some(new_entry.clone()),
                });
            }
        }
        (export_generation_increment, callbacks)
    }

    /// Copy modified values from existing vars to `vars_to_acquire`.
    fn acquire_variables(&self, vars_to_acquire: &mut VarTable) {
        for key in &self.modified {
            match self.vars.get(key) {
                None => {
                    /* The value has been deleted. */
                    vars_to_acquire.remove(key);
                }
                Some(src) => {
                    // The value has been modified. Copy it over.
                    vars_to_acquire.insert(key.clone(), src.clone());
                }
            }
        }
    }

    fn populate_1_variable(
        input: &wstr,
        flags: EnvVarFlags,
        vars: &mut VarTable,
        storage: &mut WString,
    ) -> bool {
        let s = skip_spaces(input);
        let Some(colon) = s.chars().position(|c| c == ':') else {
            return false;
        };

        // Parse out the value into storage, and decode it into a variable.
        storage.clear();
        let Some(unescaped) = unescape_string(
            &s[colon + 1..],
            UnescapeStringStyle::Script(UnescapeFlags::default()),
        ) else {
            return false;
        };
        *storage = unescaped;
        let var = EnvVar::new_vec(decode_serialized(&*storage), flags);

        // Parse out the key and write into the map.
        *storage = s[..colon].to_owned();
        let key = &*storage;
        (*vars).insert(key.clone(), var);
        true
    }
    /// Parse message msg per fish 3.0 format.
    fn parse_message_30_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        use fish3_uvars as f3;
        if msg.starts_with(L!("#")) {
            return;
        }

        let mut cursor = msg;
        if !r#match(&mut cursor, f3::SETUVAR) {
            flogf!(warning, PARSE_ERR, msg);
            return;
        }
        // Parse out flags.
        let mut flags = EnvVarFlags::default();
        loop {
            cursor = skip_spaces(cursor);
            if cursor.char_at(0) != '-' {
                break;
            }
            if r#match(&mut cursor, f3::EXPORT) {
                flags |= EnvVarFlags::EXPORT;
            } else if r#match(&mut cursor, f3::PATH) {
                flags |= EnvVarFlags::PATHVAR;
            } else {
                // Skip this unknown flag, for future proofing.
                while !cursor.is_empty() && !matches!(cursor.char_at(0), ' ' | '\t') {
                    cursor = &cursor[1..];
                }
            }
        }

        // Populate the variable with these flags.
        if !Self::populate_1_variable(cursor, flags, vars, storage) {
            flogf!(warning, PARSE_ERR, msg);
        }
    }

    /// Parse message msg per fish 2.x format.
    fn parse_message_2x_internal(msg: &wstr, vars: &mut VarTable, storage: &mut WString) {
        use fish2x_uvars as f2x;
        let mut cursor = msg;

        if cursor.char_at(0) == '#' {
            return;
        }
        let mut flags = EnvVarFlags::default();
        if r#match(&mut cursor, f2x::SET_EXPORT) {
            flags |= EnvVarFlags::EXPORT;
        } else if r#match(&mut cursor, f2x::SET) {
        } else {
            flogf!(warning, PARSE_ERR, msg);
            return;
        }

        if !Self::populate_1_variable(cursor, flags, vars, storage) {
            flogf!(warning, PARSE_ERR, msg);
        }
    }

    fn read_message_internal(file: &File, vars: &mut VarTable) -> UvarFormat {
        let mut contents = vec![];
        // Read everything from the file. Put a sane limit on it.
        // TODO: Ideally, the cast should be checked at compile time.
        if let Err(e) = file
            .take(u64::try_from(MAX_READ_SIZE).expect("MAX_READ_SIZE must fit into u64"))
            .read_to_end(&mut contents)
        {
            flog!(warning, "Failed to read file:", e);
        }

        // Handle overlong files.
        // We read at most `MAX_READ_SIZE` bytes due to the `take`.
        if contents.len() == MAX_READ_SIZE {
            // Back up to a newline.
            let newline = contents.iter().rposition(|c| *c == b'\n').unwrap_or(0);
            contents.truncate(newline);
        }

        Self::populate_variables(&contents, vars)
    }
}

/// Return the default variable path, or an empty string on failure.
pub fn default_vars_path() -> WString {
    if let Some(mut path) = default_vars_path_directory() {
        path.push_str("/fish_variables");
        return path;
    }
    WString::new()
}

/// Error message.
const PARSE_ERR: &wstr = L!("Unable to parse universal variable message: '%s'");

/// Small note about not editing ~/.fishd manually. Inserted at the top of all .fishd files.
const SAVE_MSG: &[u8] = b"# This file contains fish universal variable definitions.\n";

/// Version for fish 3.0
const UVARS_VERSION_3_0: &[u8] = b"3.0";

// Maximum file size we'll read.
const MAX_READ_SIZE: usize = 16 * 1024 * 1024;

// Fields used in fish 2.x uvars.

mod fish2x_uvars {
    pub const SET: &[u8] = b"SET";
    pub const SET_EXPORT: &[u8] = b"SET_EXPORT";
}
// Fields used in fish 3.0 uvars
mod fish3_uvars {
    pub const SETUVAR: &[u8] = b"SETUVAR";
    pub const EXPORT: &[u8] = b"--export";
    pub const PATH: &[u8] = b"--path";
}

/// Return the default variable path, or an empty string on failure.
fn default_vars_path_directory() -> Option<WString> {
    path_get_config()
}

/// Test if the message msg contains the command cmd.
/// On success, updates the cursor to just past the command.
fn r#match(inout_cursor: &mut &wstr, cmd: &[u8]) -> bool {
    let cursor = *inout_cursor;
    if !cmd
        .iter()
        .copied()
        .map(char::from)
        .eq(cursor.chars().take(cmd.len()))
    {
        return false;
    }
    let len = cmd.len();
    if cursor.len() != len && !matches!(cursor.char_at(len), ' ' | '\t') {
        return false;
    }
    *inout_cursor = &cursor[len..];
    true
}

/// The universal variable format has some funny escaping requirements; here we try to be safe.
fn is_universal_safe_to_encode_directly(c: char) -> bool {
    if !(32..=128).contains(&u32::from(c)) {
        return false;
    }

    c.is_alphanumeric() || matches!(c, '/' | '_')
}

/// Escape specified string.
fn full_escape(input: &wstr) -> WString {
    let mut out = WString::new();
    for c in input.chars() {
        if is_universal_safe_to_encode_directly(c) {
            out.push(c);
        } else if c.is_ascii() {
            sprintf!(=> &mut out, "\\x%.2x", u32::from(c));
        } else if let Some(encoded_byte) = decode_byte_from_char(c) {
            sprintf!(=> &mut out, "\\x%.2x", u32::from(encoded_byte));
        } else if u32::from(c) < 65536 {
            sprintf!(=> &mut out, "\\u%.4x", u32::from(c));
        } else {
            sprintf!(=> &mut out, "\\U%.8x", u32::from(c));
        }
    }
    out
}

/// Converts input to UTF-8 and appends it to receiver.
fn append_utf8(input: &wstr, receiver: &mut Vec<u8>) {
    // Notably we convert between wide and narrow strings without decoding our private-use
    // characters.
    receiver.extend_from_slice(input.to_string().as_bytes());
}

/// Creates a file entry like "SET fish_color_cwd:FF0". Appends the result to *result (as UTF8).
/// Returns true on success. storage may be used for temporary storage, to avoid allocations.
fn append_file_entry(
    flags: EnvVarFlags,
    key_in: &wstr,
    val_in: &wstr,
    result: &mut Vec<u8>,
) -> bool {
    use fish3_uvars as f3;

    // Record the length on entry, in case we need to back up.
    let mut success = true;
    let result_length_on_entry = result.len();

    // Append SETVAR header.
    result.extend_from_slice(f3::SETUVAR);
    result.push(b' ');

    // Append flags.
    if flags.contains(EnvVarFlags::EXPORT) {
        result.extend_from_slice(f3::EXPORT);
        result.push(b' ');
    }
    if flags.contains(EnvVarFlags::PATHVAR) {
        result.extend_from_slice(f3::PATH);
        result.push(b' ');
    }

    // Append variable name like "fish_color_cwd".
    if !valid_var_name(key_in) {
        flogf!(error, "Illegal variable name: '%s'", key_in);
        success = false;
    }
    if success {
        append_utf8(key_in, result);
    }

    // Append ":".
    if success {
        result.push(b':');
    }

    // Append value.
    if success {
        append_utf8(&full_escape(val_in), result);
    }

    // Append newline.
    if success {
        result.push(b'\n');
    }

    // Don't modify result on failure. It's sufficient to simply resize it since all we ever did was
    // append to it.
    if !success {
        result.truncate(result_length_on_entry);
    }

    success
}

/// Encoding of a null string.
const ENV_NULL: &wstr = L!("\x1d");

/// Character used to separate arrays in universal variables file.
/// This is 30, the ASCII record separator.
const UVAR_ARRAY_SEP: char = '\x1e';

/// Decode a serialized universal variable value into a list.
fn decode_serialized(val: &wstr) -> Vec<WString> {
    if val == ENV_NULL {
        return vec![];
    }
    val.split(UVAR_ARRAY_SEP).map(|v| v.to_owned()).collect()
}

/// Decode a a list into a serialized universal variable value.
fn encode_serialized(vals: &[WString]) -> WString {
    if vals.is_empty() {
        return ENV_NULL.to_owned();
    }
    join_strings(vals, UVAR_ARRAY_SEP)
}

fn skip_spaces(mut s: &wstr) -> &wstr {
    while s.starts_with(L!(" ")) || s.starts_with(L!("\t")) {
        s = &s[1..];
    }
    s
}

#[cfg(test)]
mod tests {
    use fish_tempfile::TempDir;

    use crate::common::ENCODE_DIRECT_BASE;
    use crate::common::bytes2wcstring;
    use crate::common::char_offset;
    use crate::common::wcs2osstring;
    use crate::env::{EnvVar, EnvVarFlags, VarTable};
    use crate::env_universal_common::{EnvUniversal, UvarFormat};
    use crate::tests::prelude::*;
    use crate::wchar::prelude::*;
    use crate::wutil::{INVALID_FILE_ID, file_id_for_path};

    const UVARS_PER_THREAD: usize = 8;

    /// Creates a unique temporary directory and file path for universal variable tests inside the
    /// new tempdir.
    /// Returns (temp_dir, file_path).
    fn make_test_uvar_path() -> std::io::Result<(TempDir, WString)> {
        let temp_dir = fish_tempfile::new_dir()?;
        let file_path = temp_dir.path().join("varsfile.txt");
        let file_path = bytes2wcstring(file_path.as_os_str().as_encoded_bytes());
        Ok((temp_dir, file_path))
    }

    fn test_universal_helper(x: usize, path: &wstr) {
        let _cleanup = test_init();
        let mut uvars = EnvUniversal::new();
        uvars.initialize_at_path(path.to_owned());

        for j in 0..UVARS_PER_THREAD {
            let key = sprintf!("key_%d_%d", x, j);
            let val = sprintf!("val_%d_%d", x, j);
            uvars.set(&key, EnvVar::new(val, EnvVarFlags::empty()));
            let (synced, _) = uvars.sync();
            assert!(
                synced,
                "Failed to sync universal variables after modification"
            );
        }

        // Last step is to delete the first key.
        uvars.remove(&sprintf!("key_%d_%d", x, 0));
        let (synced, _) = uvars.sync();
        assert!(synced, "Failed to sync universal variables after deletion");
    }

    #[test]
    fn test_universal() {
        let _cleanup = test_init();
        let (_test_dir, test_path) = make_test_uvar_path().unwrap();

        let threads = 1;
        let mut handles = Vec::new();

        for i in 0..threads {
            let path = test_path.to_owned();
            handles.push(std::thread::spawn(move || {
                test_universal_helper(i, &path);
            }));
        }

        for h in handles {
            h.join().unwrap();
        }

        let mut uvars = EnvUniversal::new();
        uvars.initialize_at_path(test_path.to_owned());

        for i in 0..threads {
            for j in 0..UVARS_PER_THREAD {
                let key = sprintf!("key_%d_%d", i, j);
                let expected_val = if j == 0 {
                    None
                } else {
                    Some(EnvVar::new(
                        sprintf!("val_%d_%d", i, j),
                        EnvVarFlags::empty(),
                    ))
                };
                let var = uvars.get(&key);
                assert_eq!(var, expected_val);
            }
        }
    }

    #[test]
    #[serial]
    fn test_universal_output() {
        let _cleanup = test_init();
        let flag_export = EnvVarFlags::EXPORT;
        let flag_pathvar = EnvVarFlags::PATHVAR;

        let mut vars = VarTable::new();
        vars.insert(
            L!("varA").to_owned(),
            EnvVar::new_vec(
                vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
                EnvVarFlags::empty(),
            ),
        );
        vars.insert(
            L!("varB").to_owned(),
            EnvVar::new_vec(vec![L!("ValB1").to_owned()], flag_export),
        );
        vars.insert(
            L!("varC").to_owned(),
            EnvVar::new_vec(vec![L!("ValC1").to_owned()], EnvVarFlags::empty()),
        );
        vars.insert(
            L!("varD").to_owned(),
            EnvVar::new_vec(vec![L!("ValD1").to_owned()], flag_export | flag_pathvar),
        );
        vars.insert(
            L!("varE").to_owned(),
            EnvVar::new_vec(
                vec![L!("ValE1").to_owned(), L!("ValE2").to_owned()],
                flag_pathvar,
            ),
        );
        vars.insert(
            L!("varF").to_owned(),
            EnvVar::new_vec(
                vec![WString::from_chars([char_offset(ENCODE_DIRECT_BASE, 0xfc)])],
                EnvVarFlags::empty(),
            ),
        );

        let text = EnvUniversal::serialize_with_vars(&vars);
        let expected = concat!(
            "# This file contains fish universal variable definitions.\n",
            "# VERSION: 3.0\n",
            "SETUVAR varA:ValA1\\x1eValA2\n",
            "SETUVAR --export varB:ValB1\n",
            "SETUVAR varC:ValC1\n",
            "SETUVAR --export --path varD:ValD1\n",
            "SETUVAR --path varE:ValE1\\x1eValE2\n",
            "SETUVAR varF:\\xfc\n",
        )
        .as_bytes();
        assert_eq!(text, expected);
    }

    #[test]
    #[serial]
    fn test_universal_parsing() {
        let _cleanup = test_init();
        let input = concat!(
            "# This file contains fish universal variable definitions.\n",
            "# VERSION: 3.0\n",
            "SETUVAR varA:ValA1\\x1eValA2\n",
            "SETUVAR --export varB:ValB1\n",
            "SETUVAR --nonsenseflag varC:ValC1\n",
            "SETUVAR --export --path varD:ValD1\n",
            "SETUVAR --path --path varE:ValE1\\x1eValE2\n",
        )
        .as_bytes();

        let flag_export = EnvVarFlags::EXPORT;
        let flag_pathvar = EnvVarFlags::PATHVAR;

        let mut vars = VarTable::new();

        vars.insert(
            L!("varA").to_owned(),
            EnvVar::new_vec(
                vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
                EnvVarFlags::empty(),
            ),
        );
        vars.insert(
            L!("varB").to_owned(),
            EnvVar::new_vec(vec![L!("ValB1").to_owned()], flag_export),
        );
        vars.insert(
            L!("varC").to_owned(),
            EnvVar::new_vec(vec![L!("ValC1").to_owned()], EnvVarFlags::empty()),
        );
        vars.insert(
            L!("varD").to_owned(),
            EnvVar::new_vec(vec![L!("ValD1").to_owned()], flag_export | flag_pathvar),
        );
        vars.insert(
            L!("varE").to_owned(),
            EnvVar::new_vec(
                vec![L!("ValE1").to_owned(), L!("ValE2").to_owned()],
                flag_pathvar,
            ),
        );

        let mut parsed_vars = VarTable::new();
        EnvUniversal::populate_variables(input, &mut parsed_vars);
        assert_eq!(vars, parsed_vars);
    }

    #[test]
    #[serial]
    fn test_universal_parsing_legacy() {
        let _cleanup = test_init();
        let input = concat!(
            "# This file contains fish universal variable definitions.\n",
            "SET varA:ValA1\\x1eValA2\n",
            "SET_EXPORT varB:ValB1\n",
        )
        .as_bytes();

        let mut vars = VarTable::new();
        vars.insert(
            L!("varA").to_owned(),
            EnvVar::new_vec(
                vec![L!("ValA1").to_owned(), L!("ValA2").to_owned()],
                EnvVarFlags::empty(),
            ),
        );
        vars.insert(
            L!("varB").to_owned(),
            EnvVar::new(L!("ValB1").to_owned(), EnvVarFlags::EXPORT),
        );

        let mut parsed_vars = VarTable::new();
        EnvUniversal::populate_variables(input, &mut parsed_vars);
        assert_eq!(vars, parsed_vars);
    }

    #[test]
    fn test_universal_callbacks() {
        let _cleanup = test_init();
        let (_test_dir, test_path) = make_test_uvar_path().unwrap();
        let mut uvars1 = EnvUniversal::new();
        let mut uvars2 = EnvUniversal::new();
        let mut callbacks = uvars1
            .initialize_at_path(test_path.to_owned())
            .unwrap_or_default();
        callbacks.append(
            &mut uvars2
                .initialize_at_path(test_path.to_owned())
                .unwrap_or_default(),
        );

        macro_rules! sync {
            ($uvars:expr) => {
                let (_, cb_opt) = $uvars.sync();
                if let Some(mut cb) = cb_opt {
                    callbacks.append(&mut cb);
                }
            };
        }

        let noflags = EnvVarFlags::empty();

        // Put some variables into both.
        uvars1.set(L!("alpha"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("beta"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("delta"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("kappa"), EnvVar::new(L!("1").to_owned(), noflags)); //
        uvars1.set(L!("omicron"), EnvVar::new(L!("1").to_owned(), noflags)); //

        sync!(uvars1);
        sync!(uvars2);

        // Change uvars1.
        uvars1.set(L!("alpha"), EnvVar::new(L!("2").to_owned(), noflags)); // changes value
        uvars1.set(
            L!("beta"),
            EnvVar::new(L!("1").to_owned(), EnvVarFlags::EXPORT),
        ); // changes export
        uvars1.remove(L!("delta")); // erases value
        uvars1.set(L!("epsilon"), EnvVar::new(L!("1").to_owned(), noflags)); // changes nothing
        sync!(uvars1);

        // Change uvars2. It should treat its value as correct and ignore changes from uvars1.
        uvars2.set(L!("lambda"), EnvVar::new(L!("1").to_owned(), noflags)); // same value
        uvars2.set(L!("kappa"), EnvVar::new(L!("2").to_owned(), noflags)); // different value

        // Now see what uvars2 sees.
        callbacks.clear();
        sync!(uvars2);

        // Sort them to get them in a predictable order.
        callbacks.sort_by(|a, b| a.key.cmp(&b.key));

        // Should see exactly three changes.
        assert_eq!(callbacks.len(), 3);
        assert_eq!(callbacks[0].key, L!("alpha"));
        assert_eq!(callbacks[0].val.as_ref().unwrap().as_string(), L!("2"));
        assert_eq!(callbacks[1].key, L!("beta"));
        assert_eq!(callbacks[1].val.as_ref().unwrap().as_string(), L!("1"));
        assert_eq!(callbacks[2].key, L!("delta"));
        assert_eq!(callbacks[2].val, None);
    }

    #[test]
    #[serial]
    fn test_universal_formats() {
        let _cleanup = test_init();
        macro_rules! validate {
            ( $version_line:literal, $expected_format:expr ) => {
                assert_eq!(
                    EnvUniversal::format_for_contents($version_line),
                    $expected_format
                );
            };
        }
        validate!(b"# VERSION: 3.0", UvarFormat::Fish_3_0);
        validate!(b"# version: 3.0", UvarFormat::Fish_2_x);
        validate!(b"# blah blahVERSION: 3.0", UvarFormat::Fish_2_x);
        validate!(b"stuff\n# blah blahVERSION: 3.0", UvarFormat::Fish_2_x);
        validate!(b"# blah\n# VERSION: 3.0", UvarFormat::Fish_3_0);
        validate!(b"# blah\n#VERSION: 3.0", UvarFormat::Fish_3_0);
        validate!(b"# blah\n#VERSION:3.0", UvarFormat::Fish_3_0);
        validate!(b"# blah\n#VERSION:3.1", UvarFormat::Future);
    }

    #[test]
    fn test_universal_ok_to_save() {
        let _cleanup = test_init();
        // Ensure we don't try to save after reading from a newer fish.
        let (_test_dir, test_path) = make_test_uvar_path().unwrap();
        let contents = b"# VERSION: 99999.99\n";
        std::fs::write(wcs2osstring(&test_path), contents).unwrap();

        let before_id = file_id_for_path(&test_path);
        assert_ne!(before_id, INVALID_FILE_ID, "test_path should be readable");

        let mut uvars = EnvUniversal::new();
        uvars
            .initialize_at_path(test_path.to_owned())
            .unwrap_or_default();
        assert!(!uvars.is_ok_to_save(), "Should not be OK to save");
        uvars.sync();
        assert!(!uvars.is_ok_to_save(), "Should still not be OK to save");
        uvars.set(
            L!("SOMEVAR"),
            EnvVar::new(L!("SOMEVALUE").to_owned(), EnvVarFlags::empty()),
        );

        // Ensure file is same.
        let after_id = file_id_for_path(&test_path);
        assert_eq!(before_id, after_id, "test_path should not have changed",);
    }
}
