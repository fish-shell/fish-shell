use crate::common::wcs2zstring;
use crate::env::{
    is_read_only, ElectricVar, EnvMode, EnvStackSetResult, EnvVar, EnvVarFlags, Statuses, VarTable,
    ELECTRIC_VARIABLES, PATH_ARRAY_SEP,
};
use crate::env_universal_common::EnvUniversal;
use crate::flog::FLOG;
use crate::global_safety::RelaxedAtomicBool;
use crate::history::{history_session_id_from_var, History};
use crate::kill::kill_entries;
use crate::null_terminated_array::OwningNullTerminatedArray;
use crate::reader::{commandline_get_state, reader_status_count};
use crate::threads::{is_forked_child, is_main_thread};
use crate::wchar::prelude::*;
use crate::wutil::fish_wcstol_radix;

use lazy_static::lazy_static;
use std::cell::{RefCell, UnsafeCell};
use std::collections::HashSet;
use std::ffi::CString;
use std::marker::PhantomData;
use std::mem;
use std::ops::{Deref, DerefMut};

use std::sync::{atomic::AtomicU64, atomic::Ordering, Arc, Mutex, MutexGuard};

/// Getter for universal variables.
/// This is typically initialized in env_init(), and is considered empty before then.
pub fn uvars() -> MutexGuard<'static, EnvUniversal> {
    use std::sync::OnceLock;
    /// Universal variables instance.
    static UVARS: OnceLock<Mutex<EnvUniversal>> = OnceLock::new();
    UVARS
        .get_or_init(|| Mutex::new(EnvUniversal::new()))
        .lock()
        .unwrap()
}

/// Whether we were launched with no_config; in this case setting a uvar instead sets a global.
pub static UVAR_SCOPE_IS_GLOBAL: RelaxedAtomicBool = RelaxedAtomicBool::new(false);

/// Apply the pathvar behavior, splitting about colons.
pub fn colon_split<T: AsRef<wstr>>(val: &[T]) -> Vec<WString> {
    let mut split_val = Vec::new();
    for str in val.iter() {
        split_val.extend(str.as_ref().split(PATH_ARRAY_SEP).map(|s| s.to_owned()));
    }
    split_val
}

/// Return true if a variable should become a path variable by default. See #436.
fn variable_should_auto_pathvar(name: &wstr) -> bool {
    name.ends_with("PATH")
}

/// We cache our null-terminated export list. However an exported variable may change for lots of
/// reasons: popping a scope, a modified universal variable, etc. We thus have a monotone counter.
/// Every time an exported variable changes in a node, it acquires the next generation. 0 is a
/// sentinel that indicates that the node contains no exported variables.
type ExportGeneration = u64;
fn next_export_generation() -> ExportGeneration {
    static GEN: AtomicU64 = AtomicU64::new(0);
    1 + GEN.fetch_add(1, Ordering::Relaxed)
}

fn set_umask(list_val: &[WString]) -> EnvStackSetResult {
    if list_val.len() != 1 || list_val[0].is_empty() {
        return EnvStackSetResult::Invalid;
    }
    let Ok(mask) = fish_wcstol_radix(&list_val[0], 8) else {
        return EnvStackSetResult::Invalid;
    };

    #[allow(
        unused_comparisons,
        clippy::manual_range_contains,
        clippy::absurd_extreme_comparisons
    )]
    if mask > 0o777 || mask < 0 {
        return EnvStackSetResult::Invalid;
    }
    // Do not actually create a umask variable. On env_stack_t::get() it will be calculated.
    // SAFETY: umask cannot fail.
    unsafe { libc::umask(mask as libc::mode_t) };
    EnvStackSetResult::Ok
}

/// A query for environment variables.
struct Query {
    /// Whether any scopes were specified.
    pub has_scope: bool,

    /// Whether to search local, function, global, universal scopes.
    pub local: bool,
    pub function: bool,
    pub global: bool,
    pub universal: bool,

    /// Whether export or unexport was specified.
    pub has_export_unexport: bool,

    /// Whether to search exported and unexported variables.
    pub exports: bool,
    pub unexports: bool,

    /// Whether pathvar or unpathvar was set.
    pub has_pathvar_unpathvar: bool,
    pub pathvar: bool,
    pub unpathvar: bool,

    /// Whether this is a "user" set.
    pub user: bool,
}

impl Query {
    /// Creates a `Query` from env mode flags.
    fn new(mode: EnvMode) -> Self {
        let has_scope = mode
            .intersects(EnvMode::LOCAL | EnvMode::FUNCTION | EnvMode::GLOBAL | EnvMode::UNIVERSAL);
        let has_export_unexport = mode.intersects(EnvMode::EXPORT | EnvMode::UNEXPORT);
        Query {
            has_scope,
            local: !has_scope || mode.contains(EnvMode::LOCAL),
            function: !has_scope || mode.contains(EnvMode::FUNCTION),
            global: !has_scope || mode.contains(EnvMode::GLOBAL),
            universal: !has_scope || mode.contains(EnvMode::UNIVERSAL),

            has_export_unexport,
            exports: !has_export_unexport || mode.contains(EnvMode::EXPORT),
            unexports: !has_export_unexport || mode.contains(EnvMode::UNEXPORT),

            // note we don't use pathvar for searches, so these don't default to true if unspecified.
            has_pathvar_unpathvar: mode.intersects(EnvMode::PATHVAR | EnvMode::UNPATHVAR),
            pathvar: mode.contains(EnvMode::PATHVAR),
            unpathvar: mode.contains(EnvMode::UNPATHVAR),

            user: mode.contains(EnvMode::USER),
        }
    }

    /// Returns whether an environment variable matches the query's export criteria.
    fn export_matches(&self, var: &EnvVar) -> bool {
        if self.has_export_unexport {
            if var.exports() {
                self.exports
            } else {
                self.unexports
            }
        } else {
            true
        }
    }

    /// Returns whether an environment variable matches the query's path variable criteria.
    fn pathvar_matches(&self, var: &EnvVar) -> bool {
        if self.has_pathvar_unpathvar {
            if var.is_pathvar() {
                self.pathvar
            } else {
                self.unpathvar
            }
        } else {
            true
        }
    }
}

// Struct representing one level in the function variable stack.
struct EnvNode {
    // Variable table.
    env: VarTable,

    /// Does this node imply a new variable scope? If yes, all non-global variables below this one
    /// in the stack are invisible. If new_scope is set for the global variable node, the universe
    /// will explode.
    new_scope: bool,

    /// The export generation. If this is nonzero, then we contain a variable that is exported to
    /// subshells, or redefines a variable to not be exported.
    export_gen: ExportGeneration,

    /// Next scope to search. This is None if this node establishes a new scope.
    next: Option<EnvNodeRef>,
}

impl EnvNode {
    fn find_entry(&self, key: &wstr) -> Option<EnvVar> {
        self.env.get(key).cloned()
    }

    fn exports(&self) -> bool {
        self.export_gen > 0
    }

    fn changed_exported(&mut self) {
        self.export_gen = next_export_generation();
    }
}

/// EnvNodeRef is a reference to an EnvNode. It may be shared between different environments.
/// The type Arc<RefCell<...>> may look suspicious, but all accesses to the EnvNode are protected by a global lock.
#[derive(Clone)]
struct EnvNodeRef(Arc<RefCell<EnvNode>>);

impl Deref for EnvNodeRef {
    type Target = RefCell<EnvNode>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl EnvNodeRef {
    fn new(is_new_scope: bool, next: Option<EnvNodeRef>) -> EnvNodeRef {
        // Accesses are protected by the global lock.
        #[allow(unknown_lints)]
        #[allow(clippy::arc_with_non_send_sync)]
        EnvNodeRef(Arc::new(RefCell::new(EnvNode {
            env: VarTable::new(),
            new_scope: is_new_scope,
            export_gen: 0,
            next,
        })))
    }

    /// Return whether this points at the same value as another node.
    fn ptr_eq(&self, other: &EnvNodeRef) -> bool {
        Arc::ptr_eq(&self.0, &other.0)
    }

    /// Cover over find_entry.
    fn find_entry(&self, key: &wstr) -> Option<EnvVar> {
        self.borrow().find_entry(key)
    }

    /// Cover over next.
    fn next(&self) -> Option<EnvNodeRef> {
        self.borrow().next.clone()
    }

    /// Helper to get an iterator over the chain of EnvNodeRefs.
    fn iter(&self) -> EnvNodeIter {
        EnvNodeIter::new(self.clone())
    }
}

// Safety: in order to do anything with an EnvNodeRef, the caller must be holding ENV_LOCK.
unsafe impl Sync for EnvNodeRef {}

/// Helper to iterate over a chain of EnvNodeRefs.
struct EnvNodeIter {
    current: Option<EnvNodeRef>,
}

impl EnvNodeIter {
    fn new(start: EnvNodeRef) -> EnvNodeIter {
        EnvNodeIter {
            current: Some(start),
        }
    }
}

impl Iterator for EnvNodeIter {
    type Item = EnvNodeRef;

    fn next(&mut self) -> Option<EnvNodeRef> {
        let current: Option<EnvNodeRef> = self.current.take();
        if let Some(ref current) = current {
            self.current = current.next();
        }
        current
    }
}

lazy_static! {
    // All accesses to the EnvNode are protected by a global lock.
    static ref GLOBAL_NODE: EnvNodeRef = EnvNodeRef::new(false, None);
}

/// Recursive helper to snapshot a series of nodes.
fn copy_node_chain(node: &EnvNodeRef) -> EnvNodeRef {
    let next = node.next().as_ref().map(copy_node_chain);
    let node = node.borrow();
    let new_node = EnvNode {
        env: node.env.clone(),
        export_gen: node.export_gen,
        new_scope: node.new_scope,
        next,
    };
    #[allow(unknown_lints)]
    #[allow(clippy::arc_with_non_send_sync)]
    EnvNodeRef(Arc::new(RefCell::new(new_node)))
}

/// A struct wrapping up parser-local variables. These are conceptually variables that differ in
/// different fish internal processes.
#[derive(Default, Clone)]
struct PerprocData {
    pwd: WString,
    statuses: Statuses,
}

#[derive(Clone)]
pub struct EnvScopedImpl {
    // A linked list of scopes.
    locals: EnvNodeRef,

    // Global scopes. There is no parent here.
    globals: EnvNodeRef,

    // Per process data.
    perproc_data: PerprocData,

    // Exported variable array used by execv.
    export_array: Option<Arc<OwningNullTerminatedArray>>,

    // Cached list of export generations corresponding to the above export_array.
    // If this differs from the current export generations then we need to regenerate the array.
    export_array_generations: Vec<ExportGeneration>,
}

impl EnvScopedImpl {
    /// Creates a new `EnvScopedImpl` with the specified local and global scopes.
    fn new(locals: EnvNodeRef, globals: EnvNodeRef) -> Self {
        EnvScopedImpl {
            locals,
            globals,
            perproc_data: PerprocData::default(),
            export_array: None,
            export_array_generations: Vec::new(),
        }
    }

    pub fn get_last_statuses(&self) -> &Statuses {
        &self.perproc_data.statuses
    }

    pub fn set_last_statuses(&mut self, s: Statuses) {
        self.perproc_data.statuses = s;
    }

    fn try_get_computed(&self, key: &wstr) -> Option<EnvVar> {
        let ev = ElectricVar::for_name(key);
        if ev.is_none() || !ev.unwrap().computed() {
            return None;
        }

        if key == "PWD" {
            Some(EnvVar::new(
                self.perproc_data.pwd.clone(),
                EnvVarFlags::EXPORT,
            ))
        } else if key == "history" {
            // Big hack. We only allow getting the history on the main thread. Note that history_t
            // may ask for an environment variable, so don't take the lock here (we don't need it).
            if !is_main_thread() {
                return None;
            }
            let history = commandline_get_state(true).history.unwrap_or_else(|| {
                let fish_history_var = self.getf(L!("fish_history"), EnvMode::default());
                let session_id = history_session_id_from_var(fish_history_var);
                History::with_name(&session_id)
            });
            return Some(EnvVar::new_from_name_vec(
                L!("history"),
                history.get_history(),
            ));
        } else if key == "fish_killring" {
            Some(EnvVar::new_from_name_vec(
                L!("fish_killring"),
                kill_entries(),
            ))
        } else if key == "pipestatus" {
            let js = &self.perproc_data.statuses;
            let mut result = Vec::with_capacity(js.pipestatus.len());
            for i in &js.pipestatus {
                result.push(i.to_wstring());
            }
            Some(EnvVar::new_from_name_vec(L!("pipestatus"), result))
        } else if key == "status" {
            let js = &self.perproc_data.statuses;
            Some(EnvVar::new_from_name(L!("status"), js.status.to_wstring()))
        } else if key == "status_generation" {
            let status_generation = reader_status_count();
            Some(EnvVar::new_from_name(
                L!("status_generation"),
                status_generation.to_wstring(),
            ))
        } else if key == "fish_kill_signal" {
            let js = &self.perproc_data.statuses;
            let signal = js.kill_signal.map_or(0, |ks| ks.code());
            Some(EnvVar::new_from_name(
                L!("fish_kill_signal"),
                signal.to_wstring(),
            ))
        } else if key == "umask" {
            // note umask() is an absurd API: you call it to set the value and it returns the old
            // value. Thus we have to call it twice, to reset the value. The env_lock protects
            // against races. Guess what the umask is; if we guess right we don't need to reset it.
            let guess: libc::mode_t = 0o022;
            // Safety: umask cannot error.
            let res: libc::mode_t = unsafe { libc::umask(guess) };
            if res != guess {
                unsafe { libc::umask(res) };
            }
            Some(EnvVar::new_from_name(L!("umask"), sprintf!("0%0.3o", res)))
        } else {
            // We should never get here unless the electric var list is out of sync with the above code.
            panic!("Unrecognized computed var name {}", key);
        }
    }

    fn try_get_local(&self, key: &wstr) -> Option<EnvVar> {
        for cur in self.locals.iter() {
            let entry = cur.find_entry(key);
            if entry.is_some() {
                return entry;
            }
        }
        None
    }

    fn try_get_function(&self, key: &wstr) -> Option<EnvVar> {
        let mut entry = None;
        let mut node = self.locals.clone();
        while let Some(next_node) = node.next() {
            node = next_node;
            // The first node that introduces a new scope is ours.
            // If this doesn't happen, we go on until we've reached the
            // topmost local scope.
            if node.borrow().new_scope {
                break;
            }
        }
        for cur in node.iter() {
            entry = cur.find_entry(key);
            if entry.is_some() {
                break;
            }
        }
        entry
    }

    fn try_get_global(&self, key: &wstr) -> Option<EnvVar> {
        self.globals.find_entry(key)
    }

    fn try_get_universal(&self, key: &wstr) -> Option<EnvVar> {
        return uvars().get(key);
    }

    pub fn getf(&self, key: &wstr, mode: EnvMode) -> Option<EnvVar> {
        let query = Query::new(mode);
        let mut result: Option<EnvVar> = None;
        // Computed variables are effectively global and can't be shadowed.
        if query.global {
            result = self.try_get_computed(key);
        }
        if result.is_none() && query.local {
            result = self.try_get_local(key);
        }
        if result.is_none() && query.function {
            result = self.try_get_function(key);
        }
        if result.is_none() && query.global {
            result = self.try_get_global(key);
        }
        if result.is_none() && query.universal {
            result = self.try_get_universal(key);
        }
        // If the user requested only exported or unexported variables, enforce that here.
        if result.is_some() && !query.export_matches(result.as_ref().unwrap()) {
            result = None;
        }
        // Same for pathvars
        if result.is_some() && !query.pathvar_matches(result.as_ref().unwrap()) {
            result = None;
        }
        result
    }

    pub fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        let query = Query::new(flags);
        let mut names: HashSet<WString> = HashSet::new();

        // Helper to add the names of variables from `envs` to names, respecting show_exported and
        // show_unexported.
        let add_keys = |envs: &VarTable, names: &mut HashSet<WString>| {
            for (key, val) in envs.iter() {
                if query.export_matches(val) {
                    names.insert(key.clone());
                }
            }
        };

        if query.local {
            for cur in self.locals.iter() {
                add_keys(&cur.borrow().env, &mut names);
            }
        }

        if query.global {
            add_keys(&self.globals.borrow().env, &mut names);
            // Add electrics.
            for ev in ELECTRIC_VARIABLES {
                let matches = if ev.exports() {
                    query.exports
                } else {
                    query.unexports
                };
                if matches {
                    names.insert(WString::from(ev.name));
                }
            }
        }

        if query.universal {
            let uni_list = uvars().get_names(query.exports, query.unexports);
            names.extend(uni_list);
        }
        names.into_iter().collect()
    }

    /// Slightly optimized implementation.
    pub fn get_pwd_slash(&self) -> WString {
        // Return "/" if PWD is missing.
        // See https://github.com/fish-shell/fish-shell/issues/5080
        let mut pwd;
        pwd = self.perproc_data.pwd.clone();
        if !pwd.ends_with('/') {
            pwd.push('/');
        }
        pwd
    }

    /// Return a copy of self, with copied locals but shared globals.
    pub fn snapshot(&self) -> EnvMutex<Self> {
        EnvMutex::new(EnvScopedImpl {
            locals: copy_node_chain(&self.locals),
            globals: self.globals.clone(),
            perproc_data: self.perproc_data.clone(),
            export_array: None,
            export_array_generations: Vec::new(),
        })
    }
}

/// Export array implementations.
impl EnvScopedImpl {
    /// Invoke a function on the current (nonzero) export generations, in order.
    fn enumerate_generations<F>(&self, mut func: F)
    where
        F: FnMut(u64),
    {
        // Our uvars generation count doesn't come from next_export_generation(), so always supply
        // it even if it's 0.
        func(uvars().get_export_generation());
        if self.globals.borrow().exports() {
            func(self.globals.borrow().export_gen);
        }
        for node in self.locals.iter() {
            if node.borrow().exports() {
                func(node.borrow().export_gen);
            }
        }
    }

    /// Return whether the current export array is empty or out-of-date.
    fn export_array_needs_regeneration(&self) -> bool {
        // Check if our export array is stale. If we don't have one, it's obviously stale. Otherwise,
        // compare our cached generations with the current generations. If they don't match exactly then
        // our generation list is stale.
        if self.export_array.is_none() {
            return true;
        }

        let mut cursor = self.export_array_generations.iter().fuse();
        let mut mismatch = false;
        self.enumerate_generations(|gen| {
            if cursor.next().cloned() != Some(gen) {
                mismatch = true;
            }
        });
        if cursor.next().is_some() {
            mismatch = true;
        }
        return mismatch;
    }

    /// Get the exported variables into a variable table.
    fn get_exported(n: &EnvNodeRef, table: &mut VarTable) {
        let n = n.borrow();

        // Allow parent scopes to populate first, since we may want to overwrite those results.
        if let Some(next) = n.next.as_ref() {
            Self::get_exported(next, table);
        }

        for (key, var) in n.env.iter() {
            if var.exports() {
                // Export the variable. Note this overwrites existing values from previous scopes.
                table.insert(key.clone(), var.clone());
            } else {
                // We need to erase from the map if we are not exporting, since a lower scope may have
                // exported. See #2132.
                table.remove(key);
            }
        }
    }

    /// Return a newly allocated export array.
    fn create_export_array(&self) -> Arc<OwningNullTerminatedArray> {
        FLOG!(env_export, "create_export_array() recalc");
        let mut vals = VarTable::new();
        Self::get_exported(&self.globals, &mut vals);
        Self::get_exported(&self.locals, &mut vals);

        let uni = uvars().get_names(true, false);
        for key in uni {
            let var = uvars().get(&key).unwrap();
            // Only insert if not already present, as uvars have lowest precedence.
            // TODO: a longstanding bug is that an unexported local variable will not mask an exported uvar.
            vals.entry(key).or_insert(var);
        }

        // Dorky way to add our single exported computed variable.
        vals.insert(
            L!("PWD").to_owned(),
            EnvVar::new_from_name(L!("PWD"), self.perproc_data.pwd.clone()),
        );

        // Construct the export list: a list of strings of the form key=value.
        let mut export_list: Vec<CString> = Vec::with_capacity(vals.len());
        for (key, val) in vals.into_iter() {
            let mut str = key;
            str.push('=');
            str.push_utfstr(&val.as_string());
            export_list.push(wcs2zstring(&str));
        }
        return Arc::new(OwningNullTerminatedArray::new(export_list));
    }

    // Exported variable array used by execv.
    pub fn export_array(&mut self) -> Arc<OwningNullTerminatedArray> {
        assert!(!is_forked_child());
        if self.export_array_needs_regeneration() {
            self.export_array = Some(self.create_export_array());

            // Have to pull this into a local to satisfy the borrow checker.
            let mut generations = std::mem::take(&mut self.export_array_generations);
            self.enumerate_generations(|gen| generations.push(gen));
            self.export_array_generations = generations;
        }
        return self.export_array.as_ref().unwrap().clone();
    }
}

#[derive(Copy, Clone, Default)]
/// A restricted set of variable flags.
struct VarFlags {
    /// If set, whether the variable should be a path variable; otherwise guess based on the name.
    pub pathvar: Option<bool>,

    /// If set, the new export value; otherwise inherit any existing export value.
    pub exports: Option<bool>,

    /// Whether the variable is exported by some parent.
    pub parent_exports: bool,
}

#[derive(Copy, Clone, Default)]
pub struct ModResult {
    /// The publicly visible status of the set call.
    pub status: EnvStackSetResult,

    /// Whether the global scope was modified.
    pub global_modified: bool,

    /// Whether universal variables were modified.
    pub uvar_modified: bool,
}

impl ModResult {
    /// Creates a `ModResult` with a given status.
    fn new(status: EnvStackSetResult) -> Self {
        ModResult {
            status,
            ..Default::default()
        }
    }
}

/// A mutable "subclass" of EnvScopedImpl.
#[derive(Clone)]
pub struct EnvStackImpl {
    pub base: EnvScopedImpl,

    /// The scopes of caller functions, which are currently shadowed.
    shadowed_locals: Vec<EnvNodeRef>,
}

impl EnvStackImpl {
    /// Return a new impl representing global variables, with a single local scope.
    pub fn new() -> EnvMutex<EnvStackImpl> {
        let globals = GLOBAL_NODE.clone();
        let locals = EnvNodeRef::new(false, None);
        let base = EnvScopedImpl::new(locals, globals);
        EnvMutex::new(EnvStackImpl {
            base,
            shadowed_locals: Vec::new(),
        })
    }

    /// Set a variable under the name `key`, using the given `mode`, setting its value to `val`.
    pub fn set(&mut self, key: &wstr, mode: EnvMode, mut val: Vec<WString>) -> ModResult {
        let query = Query::new(mode);
        // Handle electric and read-only variables.
        if let Some(ret) = self.try_set_electric(key, &query, &mut val) {
            return ModResult::new(ret);
        }

        // Resolve as much of our flags as we can. Note these contain maybes, and we may defer the final
        // decision until the set_in_node call. Also note that we only inherit pathvar, not export. For
        // example, if you have a global exported variable, a local variable with the same name will not
        // automatically be exported. But if you have a global pathvar, a local variable with the same
        // name will be a pathvar. This is historical.
        let mut flags = VarFlags::default();
        if let Some(existing) = self.find_variable(key) {
            flags.pathvar = Some(existing.is_pathvar());
            flags.parent_exports = existing.exports();
        }
        if query.has_export_unexport {
            flags.exports = Some(query.exports);
        }
        if query.has_pathvar_unpathvar {
            flags.pathvar = Some(query.pathvar);
        }

        let mut result = ModResult::new(EnvStackSetResult::Ok);
        if query.has_scope {
            // The user requested a particular scope.
            // If we don't have uvars, fall back to using globals.
            if query.universal && !UVAR_SCOPE_IS_GLOBAL.load() {
                self.set_universal(key, val, query);
                result.uvar_modified = true;
            } else if query.global || (query.universal && UVAR_SCOPE_IS_GLOBAL.load()) {
                Self::set_in_node(&mut self.base.globals, key, val, flags);
            } else if query.local {
                assert!(
                    !self.base.locals.ptr_eq(&self.base.globals),
                    "Locals should not be globals"
                );
                Self::set_in_node(&mut self.base.locals, key, val, flags);
            } else if query.function {
                // "Function" scope is:
                // Either the topmost local scope of the nearest function,
                // or the top-level local scope if no function exists.
                //
                // This is distinct from the unspecified scope,
                // which is the global scope if no function exists.
                let mut node = self.base.locals.clone();
                while node.next().is_some() {
                    node = node.next().unwrap();
                    // The first node that introduces a new scope is ours.
                    // If this doesn't happen, we go on until we've reached the
                    // topmost local scope.
                    if node.borrow().new_scope {
                        break;
                    }
                }
                Self::set_in_node(&mut node, key, val, flags);
            } else {
                panic!("Unknown scope");
            }
        } else if let Some(mut node) = Self::find_in_chain(&self.base.locals, key) {
            // Existing local variable.
            Self::set_in_node(&mut node, key, val, flags);
        } else if let Some(mut node) = Self::find_in_chain(&self.base.globals, key) {
            // Existing global variable.
            Self::set_in_node(&mut node, key, val, flags);
            result.global_modified = true;
        } else if !UVAR_SCOPE_IS_GLOBAL.load() && uvars().get(key).is_some() {
            // Existing universal variable.
            self.set_universal(key, val, query);
            result.uvar_modified = true;
        } else {
            // Unspecified scope with no existing variables.
            let mut node = self.resolve_unspecified_scope();
            Self::set_in_node(&mut node, key, val, flags);
            result.global_modified = node.ptr_eq(&self.base.globals);
        }
        result
    }

    /// Remove a variable under the name `key`.
    pub fn remove(&mut self, key: &wstr, mode: EnvMode) -> ModResult {
        let query = Query::new(mode);
        // Users can't remove read-only keys.
        if query.user && is_read_only(key) {
            return ModResult::new(EnvStackSetResult::Scope);
        }

        // Helper to invoke remove_from_chain and map a false return to not found.
        fn remove_from_chain(node: &mut EnvNodeRef, key: &wstr) -> EnvStackSetResult {
            if EnvStackImpl::remove_from_chain(node, key) {
                EnvStackSetResult::Ok
            } else {
                EnvStackSetResult::NotFound
            }
        }

        let mut result = ModResult::new(EnvStackSetResult::Ok);
        if query.has_scope {
            // The user requested erasing from a particular scope.
            if query.universal {
                if uvars().remove(key) {
                    result.status = EnvStackSetResult::Ok;
                } else {
                    result.status = EnvStackSetResult::NotFound;
                }
                // Note we have historically set this even if the uvar is not found.
                result.uvar_modified = true;
            } else if query.global {
                result.status = remove_from_chain(&mut self.base.globals, key);
                result.global_modified = true;
            } else if query.local {
                result.status = remove_from_chain(&mut self.base.locals, key);
            } else if query.function {
                let mut node = self.base.locals.clone();
                while node.next().is_some() {
                    node = node.next().unwrap();
                    if node.borrow().new_scope {
                        break;
                    }
                }
                result.status = remove_from_chain(&mut node, key);
            } else {
                panic!("Unknown scope");
            }
        } else if Self::remove_from_chain(&mut self.base.locals, key) {
            // pass
        } else if Self::remove_from_chain(&mut self.base.globals, key) {
            result.global_modified = true;
        } else if uvars().remove(key) {
            result.uvar_modified = true;
        } else {
            result.status = EnvStackSetResult::NotFound;
        }
        result
    }

    /// Push a new shadowing local scope.
    pub fn push_shadowing(&mut self) {
        // Propagate local exported variables.
        let node = EnvNodeRef::new(true, None);
        for cursor in self.base.locals.iter() {
            for (key, val) in cursor.borrow().env.iter() {
                if val.exports() {
                    let mut node_ref = node.borrow_mut();
                    // Do NOT overwrite existing values, since we go from inner scopes outwards.
                    if !node_ref.env.contains_key(key) {
                        node_ref.env.insert(key.clone(), val.clone());
                    }
                    node_ref.changed_exported();
                }
            }
        }
        let old_locals = mem::replace(&mut self.base.locals, node);
        self.shadowed_locals.push(old_locals);
    }

    /// Push a new non-shadowing (inner) local scope.
    pub fn push_nonshadowing(&mut self) {
        self.base.locals = EnvNodeRef::new(false, Some(self.base.locals.clone()));
    }

    /// Pop the variable stack.
    /// Return a list of the names of variables which were modified.
    /// TODO: We return the variable names because we may need to dispatch changes,
    /// for example if there is a local change to LC_ALL; but that is rare. How can
    /// we avoid these copies in the common case?
    pub fn pop(&mut self) -> Vec<WString> {
        let popped: EnvNodeRef;
        if let Some(next) = self.base.locals.next() {
            popped = mem::replace(&mut self.base.locals, next);
        } else {
            // Exhausted the inner scopes, put back a shadowing scope.
            if let Some(shadowed) = self.shadowed_locals.pop() {
                popped = mem::replace(&mut self.base.locals, shadowed);
            } else {
                panic!("Attempt to pop last local scope")
            }
        }
        let var_names = popped.borrow().env.keys().cloned().collect();
        var_names
    }

    /// Find the first node in the chain starting at `node` which contains the given key `key`.
    fn find_in_chain(node: &EnvNodeRef, key: &wstr) -> Option<EnvNodeRef> {
        #[allow(clippy::manual_find)]
        for cur in node.iter() {
            if cur.borrow().env.contains_key(key) {
                return Some(cur);
            }
        }
        None
    }

    /// Remove a variable from the chain `node`.
    /// Return true if the variable was found and removed.
    fn remove_from_chain(node: &mut EnvNodeRef, key: &wstr) -> bool {
        for cur in node.iter() {
            let mut cur_ref = cur.borrow_mut();
            if let Some(var) = cur_ref.env.remove(key) {
                if var.exports() {
                    cur_ref.changed_exported();
                }
                return true;
            }
        }
        false
    }

    /// Try setting `key` as an electric or readonly variable, whose value is provided by reference in `val`.
    /// Return an error code, or NOne if not an electric or readonly variable.
    /// `val` will not be modified upon a None return.
    fn try_set_electric(
        &mut self,
        key: &wstr,
        query: &Query,
        val: &mut Vec<WString>,
    ) -> Option<EnvStackSetResult> {
        // Do nothing if not electric.
        let ev = ElectricVar::for_name(key)?;

        // If a variable is electric, it may only be set in the global scope.
        if query.has_scope && !query.global {
            return Some(EnvStackSetResult::Scope);
        }

        // If the variable is read-only, the user may not set it.
        if query.user && ev.readonly() {
            return Some(EnvStackSetResult::Perm);
        }

        // Be picky about exporting.
        if query.has_export_unexport {
            let matches = if ev.exports() {
                query.exports
            } else {
                query.unexports
            };
            if !matches {
                return Some(EnvStackSetResult::Scope);
            }
        }

        // Handle computed mutable electric variables.
        if key == "umask" {
            return Some(set_umask(val));
        } else if key == "PWD" {
            assert!(val.len() == 1, "Should have exactly one element in PWD");
            let pwd = val.pop().unwrap();
            if pwd != self.base.perproc_data.pwd {
                self.base.perproc_data.pwd = pwd;
                self.base.globals.borrow_mut().changed_exported();
            }
            return Some(EnvStackSetResult::Ok);
        }
        // Claim the value.
        let val = std::mem::take(val);

        // Decide on the mode and set it in the global scope.
        let flags = VarFlags {
            exports: Some(ev.exports()),
            parent_exports: ev.exports(),
            pathvar: Some(false),
        };
        Self::set_in_node(&mut self.base.globals, key, val, flags);
        return Some(EnvStackSetResult::Ok);
    }

    /// Set a universal variable, inheriting as applicable from the given old variable.
    fn set_universal(&mut self, key: &wstr, mut val: Vec<WString>, query: Query) {
        let mut locked_uvars = uvars();
        let oldvar = locked_uvars.get(key);
        let oldvar = oldvar.as_ref();

        // Resolve whether or not to export.
        let mut exports = false;
        if query.has_export_unexport {
            exports = query.exports;
        } else if oldvar.is_some() {
            exports = oldvar.unwrap().exports();
        }

        // Resolve whether to be a path variable.
        // Here we fall back to the auto-pathvar behavior.
        let pathvar;
        if query.has_pathvar_unpathvar {
            pathvar = query.pathvar;
        } else if oldvar.is_some() {
            pathvar = oldvar.unwrap().is_pathvar();
        } else {
            pathvar = variable_should_auto_pathvar(key);
        }

        // Split about ':' if it's a path variable.
        if pathvar {
            val = colon_split(&val);
        }

        // Construct and set the new variable.
        let mut varflags = EnvVarFlags::empty();
        varflags.set(EnvVarFlags::EXPORT, exports);
        varflags.set(EnvVarFlags::PATHVAR, pathvar);
        let new_var = EnvVar::new_vec(val, varflags);

        locked_uvars.set(key, new_var);
    }

    /// Set a variable in a given node `node`.
    fn set_in_node(node: &mut EnvNodeRef, key: &wstr, mut val: Vec<WString>, flags: VarFlags) {
        // Read the var from the node. In C++ this was node->env[key] which establishes a default.
        let mut node_ref = node.borrow_mut();
        let var = node_ref.env.entry(key.to_owned()).or_default();

        // Use an explicit exports, or inherit from the existing variable.
        let res_exports = match flags.exports {
            Some(exports) => exports,
            None => var.exports(),
        };

        // Pathvar is inferred from the name. If set, split our entry about colons.
        let res_pathvar = match flags.pathvar {
            Some(pathvar) => pathvar,
            None => variable_should_auto_pathvar(key),
        };
        if res_pathvar {
            val = colon_split(&val);
        }

        *var = var
            .setting_vals(val)
            .setting_exports(res_exports)
            .setting_pathvar(res_pathvar);

        // Perhaps mark that this node contains an exported variable, or shadows an exported variable.
        // If so regenerate the export list.
        if res_exports || flags.parent_exports {
            node_ref.changed_exported();
        }
    }

    // Implement the default behavior of 'set' by finding the node for an unspecified scope.
    fn resolve_unspecified_scope(&mut self) -> EnvNodeRef {
        for cursor in self.base.locals.iter() {
            if cursor.borrow().new_scope {
                return cursor;
            }
        }
        return self.base.globals.clone();
    }

    /// Get an existing variable, or None.
    /// This is used for inheriting pathvar and export status.
    fn find_variable(&self, key: &wstr) -> Option<EnvVar> {
        let mut node = Self::find_in_chain(&self.base.locals, key);
        if node.is_none() {
            node = Self::find_in_chain(&self.base.globals, key);
        }
        if let Some(node) = node {
            let iter = node.borrow().env.get(key).cloned();
            assert!(iter.is_some(), "Node should contain key");
            return iter;
        }
        None
    }

    pub fn getf(&self, key: &wstr, mode: EnvMode) -> Option<EnvVar> {
        self.base.getf(key, mode)
    }

    pub fn get_names(&self, flags: EnvMode) -> Vec<WString> {
        self.base.get_names(flags)
    }

    pub fn get_pwd_slash(&self) -> WString {
        self.base.get_pwd_slash()
    }
}

// This is a big dorky lock we take around everything. Everything exported from this module should be
// wrapped in an EnvMutexGuard using this lock.
// Fine grained locking is annoying here because nodes may be shared between stacks, so each
// node would need its own lock, and each stack would need to take all the locks before any operation.
static ENV_LOCK: Mutex<()> = Mutex::new(());

/// Like MutexGuard but for our global lock.
pub struct EnvMutexGuard<'a, T: 'a> {
    _guard: MutexGuard<'static, ()>,
    value: *mut T,
    _phantom: PhantomData<&'a T>,
}

impl<'a, T: 'a> Deref for EnvMutexGuard<'a, T> {
    type Target = T;
    fn deref(&self) -> &'a T {
        // Safety: we hold the global lock.
        unsafe { &*self.value }
    }
}

impl<'a, T: 'a> DerefMut for EnvMutexGuard<'a, T> {
    fn deref_mut(&mut self) -> &'a mut T {
        // Safety: we hold the global lock.
        unsafe { &mut *self.value }
    }
}

// Like Mutex, but references the global lock.
pub struct EnvMutex<T> {
    inner: UnsafeCell<T>,
}

impl<T> EnvMutex<T> {
    pub fn new(inner: T) -> Self {
        Self {
            inner: UnsafeCell::new(inner),
        }
    }

    pub fn lock(&self) -> EnvMutexGuard<T> {
        let guard = ENV_LOCK.lock().unwrap();
        // Safety: we have the global lock.
        let value = unsafe { &mut *self.inner.get() };
        EnvMutexGuard {
            _guard: guard,
            value,
            _phantom: PhantomData,
        }
    }
}

// Safety: we use a global lock.
unsafe impl<T> Sync for EnvMutex<T> {}
unsafe impl<T> Send for EnvMutex<T> {}

#[test]
fn test_colon_split() {
    assert_eq!(colon_split(&[L!("foo")]), &[L!("foo")]);
    assert_eq!(
        colon_split(&[L!("foo:bar:baz")]),
        &[L!("foo"), L!("bar"), L!("baz")]
    );
    assert_eq!(
        colon_split(&[L!("foo:bar"), L!("baz")]),
        &[L!("foo"), L!("bar"), L!("baz")]
    );
    assert_eq!(
        colon_split(&[L!("foo:bar"), L!("baz")]),
        &[L!("foo"), L!("bar"), L!("baz")]
    );
    assert_eq!(
        colon_split(&[L!("1:"), L!("2:"), L!(":3:")]),
        &[L!("1"), L!(""), L!("2"), L!(""), L!(""), L!("3"), L!("")]
    );
}
