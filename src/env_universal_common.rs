#![allow(clippy::bad_bit_mask)]

use crate::common::{
    unescape_string, valid_var_name, wcs2zstring, UnescapeFlags, UnescapeStringStyle,
};
use crate::env::{EnvVar, EnvVarFlags, VarTable};
use crate::flog::{FLOG, FLOGF};
use crate::fs::{lock_and_load, rewrite_via_temporary_file, PotentialUpdate};
use crate::path::path_get_config;
use crate::wchar::{decode_byte_from_char, prelude::*};
use crate::wcstringutil::{join_strings, LineIterator};
use crate::wutil::{file_id_for_file, file_id_for_path_narrow, wrealpath, FileId, INVALID_FILE_ID};
use std::collections::hash_map::Entry;
use std::collections::HashSet;
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
// This is exposed for testing.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UvarFormat {
    fish_2_x,
    fish_3_0,
    future,
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
    /// This is exposed for testing only.
    pub fn initialize_at_path(&mut self, path: WString) -> Option<CallbackDataList> {
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

        FLOG!(uvar_file, "universal log sync");
        // If we have no changes, just load.
        if self.modified.is_empty() {
            let callbacks = self.load_from_path_narrow();
            FLOG!(uvar_file, "universal log no modifications");
            return (false, callbacks);
        }

        FLOG!(uvar_file, "universal log performing full sync");

        let rewrite = |old_file: &File,
                       tmp_file: &mut File|
         -> std::io::Result<PotentialUpdate<Option<UniversalReadUpdate>>> {
            match self.load_from_file(old_file) {
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
                FLOG!(uvar_file, "universal log sync failed:", e);
                (false, None)
            }
        }
    }

    /// Populate a variable table `out_vars` from a `s` string.
    /// This is exposed for testing only.
    /// Return the format of the file that we read.
    pub fn populate_variables(s: &[u8], out_vars: &mut VarTable) -> UvarFormat {
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
                UvarFormat::fish_2_x => {
                    Self::parse_message_2x_internal(&wide_line, out_vars, &mut storage);
                }
                UvarFormat::fish_3_0 => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
                // For future formats, just try with the most recent one.
                UvarFormat::future => {
                    Self::parse_message_30_internal(&wide_line, out_vars, &mut storage);
                }
            }
        }
        format
    }

    /// Guess a file format. Exposed for testing only.
    /// Return the format corresponding to file contents `s`.
    pub fn format_for_contents(s: &[u8]) -> UvarFormat {
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
                    b"# VERSION: %64s\0".as_ptr().cast(),
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
                UvarFormat::fish_3_0
            } else {
                UvarFormat::future
            };
        }
        // No version found, assume 2.x
        return UvarFormat::fish_2_x;
    }

    /// Serialize a variable list. Exposed for testing only.
    pub fn serialize_with_vars(vars: &VarTable) -> Vec<u8> {
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

    /// Exposed for testing only.
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
            FLOG!(uvar_file, "universal log sync elided based on fast stat()");
            return None;
        }

        FLOG!(uvar_file, "universal log reading from file");
        match lock_and_load(&self.vars_path, |f| {
            Ok(self.load_from_file(f).map(|update| update.data))
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
                FLOG!(uvar_file, "Failed to load from universal variable file:", e);
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
    fn load_from_file(&self, file: &File) -> Option<PotentialUpdate<UniversalReadUpdate>> {
        // Get the dev / inode.
        let current_file_id = file_id_for_file(file);
        if current_file_id == self.last_read_file_id {
            FLOG!(uvar_file, "universal log sync elided based on fstat()");
            None
        } else {
            // Read a variables table from the file.
            let mut new_vars = VarTable::new();
            let format = Self::read_message_internal(file, &mut new_vars);

            // Hacky: if the read format is in the future, avoid overwriting the file: never try to
            // save.
            let do_save = format != UvarFormat::future;

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
            FLOGF!(warning, PARSE_ERR, msg);
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
            FLOGF!(warning, PARSE_ERR, msg);
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
            FLOGF!(warning, PARSE_ERR, msg);
            return;
        }

        if !Self::populate_1_variable(cursor, flags, vars, storage) {
            FLOGF!(warning, PARSE_ERR, msg);
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
            FLOG!(warning, "Failed to read file:", e);
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
const PARSE_ERR: &wstr = L!("Unable to parse universal variable message: '%ls'");

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
        FLOGF!(error, "Illegal variable name: '%ls'", key_in);
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
