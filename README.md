# sqlite3-ulid
An extension for SQLite3 to use ULID

| OS      | Supported |
| ------- | --------- |
| Linux   | Yes       |
| Windows | No (TODO) |
| Mac     | No (TODO) |
| FreeBSD | No (TODO) |

## Functions

- [ULID_NEW](#ulid_new)
- [ULID_ENCODE](#ulid_encode)
- [ULID_DECODE](#ulid_decode)

### `ULID_NEW`

Generate new ulid value

Syntax:

```
ULID_NEW() => BLOB

ULID_NEW(INTEGER timestamp) => BLOB

ULID_NEW(INTEGER timestamp, BLOB randomness) => BLOB
```

Parameters:

- `timestamp: INTEGER`
    - Unix epoch in milliseconds. If omitted, current time is used
- `randomness: BLOB`
    - Byte array for randomness that has at least 10 bytes. If omitted, random value is used

Returns:

- ULID as byte array representation

Example:

```
SELECT ULID_NEW();

SELECT ULID_NEW(STRFTIME('%s') * 1000);

SELECT ULID_NEW(STRFTIME('%s') * 1000, RANDOMBLOB(10));
```

### `ULID_ENCODE`

Encode ulid to string format

Syntax:

```
ULID_ENCODE(BLOB ulid) => TEXT
```

Parameters:

- `ulid: BLOB`
    - ULID value

Returns:

- Encoded string of ulid

Example:

```
SELECT ULID_ENCODE(ULID_NEW());
```

### `ULID_DECODE`

Decode string encoded ulid to byte array format

Syntax:

```
ULID_DECODE(TEXT ulid_str) => BLOB
```

Parameters:

- `ulid_str: TEXT`
    - String encoded ulid value

Returns:

- ULID as byte array

Example:

```
SELECT ULID_DECODE('01GMSQJHVQ9JCDN0P4WXYQR5WC');
```

# Use extension

## Build

```
# Clone repository (including submodules)
git clone --recursive https://github.com/tsujio/sqlite3-ulid

cd sqlite3-ulid

make

# Now you can find the extension library (libsqlite-ulid.so) in the working directory
ls libsqlite-ulid.so
```

## Load

SQLite command line example:

```
# Start SQLite cli
sqlite3 db.sqlite

# Please replace 'path/to' to your environment
sqlite> .load path/to/libsqlite-ulid

# You can use extension functions
sqlite> SELECT ULID_NEW();
```

# References

- SQLite
    - https://www.sqlite.org/
- ULID
    - https://github.com/ulid/spec
