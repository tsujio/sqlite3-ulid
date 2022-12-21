# sqlite3-ulid

An extension for SQLite3 to use ULID

```
$ sqlite3 :memory:

sqlite> .load ./libsqlite-ulid

sqlite> SELECT ULID_ENCODE(ULID_NEW());
01GMTD49PVS01C9DFCWKPF71VK

sqlite> WITH
   ...> t AS (
   ...>   SELECT ULID_NEW() AS id
   ...> )
   ...> SELECT ULID_DECODE(ULID_ENCODE(id)) = id FROM t;
1
```

# Functions

- [ULID_NEW](#ulid_new)
- [ULID_ENCODE](#ulid_encode)
- [ULID_DECODE](#ulid_decode)

## `ULID_NEW`

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

## `ULID_ENCODE`

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

## `ULID_DECODE`

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

SQLite command line example:

```
# Get extension from release
wget https://github.com/tsujio/sqlite3-ulid/releases/download/v1.0.0/libsqlite-ulid-1.0.0.so -O libsqlite-ulid.so

# Start SQLite cli
sqlite3 :memory:

# Load extension ('.so' is not needed)
sqlite> .load ./libsqlite-ulid

# You can use functions in extension
sqlite> SELECT ULID_NEW();
```

# Build

```
git clone --recursive https://github.com/tsujio/sqlite3-ulid.git

cd sqlite3-ulid

make
```

# References

- SQLite
    - https://www.sqlite.org/
- ULID
    - https://github.com/ulid/spec
