This describes the fish history architecture, and upcoming JSON Lines-based fish history file format.

## History files

The fish history file stores commands the user has entered, and associated metadata. History can be displayed and queried by the user, and supports autosuggestions, offering to complete the user's command line with previously run commands.

By default, all sessions append items to a single file. The user can set the `fish_history` variable create a separate named session with its own file.

fish appends to the history file (via `O_APPEND`) after each interactive command is run. fish also attempts to take a file lock; however this is best effort as some filesystems (NFS in particular) do not offer reliable file locking. The file is designed so that, even if one item is corrupted, the rest are unaffected.

This file is periodically "vaccuumed," which means discarding items (according to some criteria, such as staleness) to reduce the file size.

### File parsing

fish at startup quickly scans the history file and records only the offsets of the record delimeters. These offsets form an index into the history file, allowing fish to locate individual records quickly, and parse them lazily. This avoids a "big up-front parse" at launch time.

With the JSONL format, fish also groups these offsets by item id as part of the startup scan, since an id can be read without parsing the entire record. Given an id, fish then constructs the item by reading the records with that id, in order, and using them to populate the fields of the item. Unknown fields in a record are retained but ignored, so a newer history file can still be read by an older fish.

### YAML-like file format (historical)

Starting with fish 2.0.0, the fish history file format is "YAML-like" meaning it is superficially similar to YAML but differs from it in key ways (such as failing to escape colons) which makes it invalid to parse with a standard YAML parser. This is a historical implementation mistake that has been preserved.

In this format, very little metadata was recorded. Items were deduplicated without concern for metadata. Running the same command ten times would record it once in the file.

## JSONL file format

In an upcoming version of fish, the file format will be switched to [JSON Lines](https://jsonlines.org), also known as NDJSON ("newline-delimited JSON"). Record boundaries are then simply newlines.

Each line contains a separate record. Should any record become corrupted (e.g. a torn write on NFS), the file parser simply advances to the next newline and continue from there.

Items are no longer deduplicated. Multiple items can share a command.

### History item IDs

A fish history item is identified by its "id:" a 64 bit unsigned integer, which contains a 48 bit _timestamp_ and a 16 bit _nonce_.

```
  Bits 63                                                  16 15             0
  +----------------------------------------------------------+---------------+
  |                     timestamp (48 bits)                  |  nonce (16 b) |
  +----------------------------------------------------------+---------------+
```

This design is similar to a [ULID](https://github.com/ulid/spec).

This timestamp gives millisecond precision over nearly 9000 years. The nonce is randomized per millisecond, and incremented within a millisecond. So ids within a single session are monotone increasing, and collisions between sessions are very unlikely: the user would need to run dozens of commands in different sessions all in the same millisecond.

The id lets fish append new metadata as it arrives. For example, one record will be emitted when a command starts. When it finishes, it emits a second record with the same id, recording its duration and exit status.

IDs are stored as base64, such as `"AZ9StuUE8AY"`. Note that sorting by id also orders items chronologically.

### History item records

A history item is represented by a collection of records, each sharing the same item ID. The initial record contains the command itself; records later in the history file may annotate the item with additional metadata.

Note records may be physically interleaved in the file. Here the user ran `sleep 5` and then (in a different session) ran `cat file.txt`.

```json
{"id":"AZ9TBVE32AQ","cmd":"sleep 5","cwd":"~","sid":"AAAo5zdKbM4"}
{"id":"AZ9TBVz27qQ","cmd":"cat file.txt","cwd":"~","sid":"AAA44pHL-_o"}
{"id":"AZ9TBVz27qQ","paths":["file.txt"]}
{"id":"AZ9TBVz27qQ","exit":0,"dur":4}
{"id":"AZ9TBVE32AQ","exit":0,"dur":5009}
```

### Initial keys

The planned keys in the history file format are:

- `id` - the history item ID
- `cmd` - the command text
- `cwd` - the current working directory at command execution, with `$HOME` replaced with `~`
- `exit` - the command’s exit code
- `dur` - execution duration in milliseconds
- `paths` - the list of arguments that resolved to paths, used for autosuggestion hinting
- `sid` - A session identifier unique to each fish instance, used to attach commands to each session

Additional metadata keys may be introduced in future versions.

### Vacuuming

Occasionally fish will _vacuum_ (rewrite) the file to reduce its size. Vacuuming is triggered by:

1. The file growing past some item count limit (working guess 1 million)
2. The user deletes some items
3. Periodically with some (TBD) cadence

Vacuuming does the following:

1. Collapses multiple records for the same ID into a single record
2. Reorders file records by id, putting the file into chronological order
3. Removes oldest items to enforce the history size limit

Vacuuming occurs automatically, by writing an adjacent file and atomically moving it into place.

After vacuuming, the above records look like:

```json
{"id":"AZ9TBVE32AQ","cmd":"sleep 5","cwd":"~","exit":0,"dur":5009,"sid":"AAAo5zdKbM4"}
{"id":"AZ9TBVz27qQ","cmd":"cat file.txt","paths":["file.txt"],"cwd":"~","exit":0,"dur":4,"sid":"AAA44pHL-_o"}
```

Records with the same id have been merged into a single record.

## Compatibility and migration

To avoid breaking old versions of fish, the history file path will gain a "jsonl" extension, i.e. change from `~/.local/share/fish/fish_history` to `~/.local/share/fish/fish_history.jsonl`

When fish launches with the new history format, if there is no file at the new path, it will populate it from the previous (extensionless) path. It then prints a message, perhaps:

> The fish history file has been migrated to `~/.local/share/fish/fish_history.jsonl`
> It is safe to delete the previous file at `~/.local/share/fish/fish_history`
