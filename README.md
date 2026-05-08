# Datastore

In-memory key-value datastore server written in C with a RESP-style protocol parser.

## Progress So Far

- Built a TCP server that listens on port `6769` and accepts client connections.
- Added command handling for `SET`, `GET`, and `DEL`.
- Implemented RESP-style command completeness checks and parsing logic.
- Added hash table storage with collision handling via linked lists.
- Implemented key insertion, lookup, deletion, and full hash table cleanup.
- Added protocol-level responses for success, null bulk replies, and common errors.

## Current Layout

- `src/main.c`: server entrypoint.
- `src/server.c`: socket server loop and command handling.
- `src/parser.c`: RESP-style command parser.
- `src/hashtable.c`: in-memory hash table implementation.
- `include/`: public headers for server, parser, and hash table.
