# Trafix Performance Notes

## TUI Collector Cadence

Dynamic TUI panels refresh on `TUI_REFRESH_INTERVAL_MS`, which defaults to one
second. Heavy collectors should avoid work that scales as `socket_count *
process_count` inside that loop.

## Socket Owner Collection

The socket-owner panel reads `/proc/net/tcp` and `/proc/net/udp`, then resolves
socket inodes to processes by scanning `/proc/*/fd`.

The expensive part is the `/proc/*/fd` walk. Trafix now performs that scan once
per socket-owner refresh and reuses the resulting inode-to-process map for all
socket rows in that refresh. This avoids repeatedly scanning every process for
each socket row.

Benchmark command:

```sh
make benchmark
```

The `bench_socket_owners` benchmark reports:

- `socket_owner_scan`: time to build the inode-to-process map.
- `socket_owner_collect`: time to collect socket-owner rows using one map scan.
