# Datastore

A Redis-inspired, single-threaded, event-driven in-memory key-value 
store built from scratch in C. Implements a custom RESP-based protocol 
over TCP, append-only file persistence, key expiry (TTL), LRU eviction, 
and a select()-based multi-client event loop.

---

## Architecture

```
Client (nc)
        ↓  TCP
   Event Loop (select)
        ↓
  Command Parser (RESP)
        ↓
  Execution Engine
     ↙        ↘
Hash Table    AOF Logger
  ↓    ↓
 LRU  TTL
```

The server is single-threaded. All command execution is sequential — 
no locks, no race conditions. Multiple clients are handled 
concurrently via I/O multiplexing with select().

---

## Components

### TCP Server & Event Loop (`server.c`)
Uses select() to monitor multiple client sockets simultaneously 
without threads. Each iteration rebuilds the fd_set from all 
connected clients and the server socket. When a socket is ready, 
it is read non-blockingly. The server never blocks on a single client.

### RESP Protocol Parser (`parser.c`)
Implements a subset of the Redis Serialization Protocol (RESP). 
Commands arrive as length-prefixed arrays:

```
*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$6\r\nGautham\r\n
```

The parser accumulates bytes across multiple recv() calls per client 
using a per-client buffer. is_complete_resp() checks whether a full 
command has arrived before parsing begins, correctly handling TCP 
stream fragmentation.

### Hash Table (`hashtable.c`)
Open-addressing with chaining. Uses a djb2-inspired hash function 
(polynomial rolling hash, multiplier 31). Each bucket holds a linked 
list of entries for collision handling. All operations are O(1) average.

### TTL / Key Expiry
Two-strategy expiry matching Redis:

- **Lazy expiry** — on every GET, check if key is expired. Delete 
  and return nil if so. Zero overhead for non-expiring keys.
- **Active expiry** — every 100 commands, ht_purge_expired() scans 
  all buckets and removes expired entries. Prevents memory buildup 
  from unaccessed expired keys.

Expiry is stored as an absolute Unix timestamp (time_t). This ensures 
correct TTL restoration after server restart.

### LRU Eviction (`hashtable.c`)
When key count reaches max_keys, the least recently used key is 
evicted before inserting a new one.

Implemented as a doubly linked list threaded through the same Entry 
nodes used by the hash table — no extra allocation. The list head is 
the most recently used key, the tail is the least recently used.

Every GET and SET moves the accessed entry to the head in O(1). 
Eviction removes the tail in O(1).

### Persistence — Append-Only File (`persistence.c`)
Every write command (SET, DEL, EXPIRE) is appended to 
`data/appendonly.aof` immediately after modifying memory, before 
responding to the client.

```
SET name Gautham
EXPIREAT name 1700000010
DEL city
```

EXPIRE is logged as EXPIREAT (absolute timestamp) so TTLs survive 
restart correctly — replaying EXPIRE with relative seconds after a 
restart would produce wrong expiry times.

On startup, the AOF is replayed line by line to rebuild the hash 
table. Keys whose EXPIREAT timestamp is already in the past are 
deleted rather than restored.

---

## Supported Commands

| Command | Syntax | Description |
|---------|--------|-------------|
| SET | `SET key value` | Store a key-value pair |
| GET | `GET key` | Retrieve value by key |
| DEL | `DEL key` | Delete a key |
| EXPIRE | `EXPIRE key seconds` | Set expiry in seconds |
| TTL | `TTL key` | Get remaining seconds |

---

## Protocol

Commands use RESP (Redis Serialization Protocol) format.
Responses:

```
+OK\r\n              — success
-ERR message\r\n     — error
$6\r\nGautham\r\n    — bulk string value
$-1\r\n              — nil (key not found)
:5\r\n               — integer (TTL)
```

---

## Design Decisions

**Single-threaded event loop over threads**
Threading introduces locks, deadlocks, and race conditions. A 
single-threaded event loop processes commands sequentially — each 
command is atomic by design. This matches Redis's core architecture 
and simplifies correctness guarantees significantly.

**AOF over snapshots**
Snapshotting saves the entire dataset periodically — any commands 
since the last snapshot are lost on crash. AOF logs every command 
immediately, giving much stronger durability. The tradeoff is a 
larger file over time (compaction is a future improvement).

**Absolute timestamps in AOF**
Storing EXPIREAT instead of EXPIRE ensures TTLs are correct after 
restart regardless of how long the server was down. A relative 
seconds value replayed after a 1-hour downtime would give keys a 
fresh lease they shouldn't have.

**LRU via doubly linked list**
A doubly linked list gives O(1) move-to-head and O(1) tail removal. 
The list is threaded through existing Entry nodes — no separate 
allocation, no extra memory overhead per key.

---

## Build

```bash
# Requirements: gcc, make
make
```

---

## Run

```bash
./datastore
```

Server listens on port 6379 by default.

---

## Test with netcat

```bash
# SET
printf '*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$10\r\nHello World\r\n' | nc -w 1 localhost 6379

# GET
printf '*2\r\n$3\r\nGET\r\n$4\r\nname\r\n' | nc -w 1 localhost 6379

# EXPIRE
printf '*3\r\n$6\r\nEXPIRE\r\n$4\r\nname\r\n$2\r\n10\r\n' | nc -w 1 localhost 6379

# TTL
printf '*2\r\n$3\r\nTTL\r\n$4\r\nname\r\n' | nc -w 1 localhost 6379
```

---

## Benchmark

```bash
# terminal 1
./datastore

# terminal 2
./benchmark
```

### Results (Intel i5 MacBook Air)

| Operation | Count | Throughput | Latency |
|-----------|-------|------------|---------|
| SET | 10,000 | 16,541 ops/sec | 0.060 ms |
| GET | 10,000 | 42,116 ops/sec | 0.024 ms |

GET is faster than SET because SET also writes to the AOF file on 
every command. GET is a pure memory operation.

---

## File Structure

```
datastore/
├── src/
│   ├── main.c          — entry point
│   ├── server.c        — TCP server, event loop, command dispatch
│   ├── parser.c        — RESP protocol parser
│   ├── hashtable.c     — hash table, LRU list, TTL
│   ├── persistence.c   — append-only file
│   └── benchmark.c     — load testing client
├── include/
│   ├── server.h
│   ├── parser.h
│   ├── hashtable.h
│   └── persistence.h
├── data/
│   └── appendonly.aof
├── Makefile
└── README.md
```

---
