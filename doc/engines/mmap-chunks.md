# mmap-chunks

[Documentation](../README.md) · [Shared workflow](../architecture.md) · [disk-partitions](disk-partitions.md)

This engine builds one hashmap from the current chunk, then searches later
chunks for duplicates. Entries [reference mapped lines](../architecture.md#line-references),
so the table does not need its own copies of the words.

Chunks are consecutive ranges of the working file, with boundaries extended to
the next newline. Their [nominal size](../resources.md#table-and-chunk-sizing)
depends on the memory budget and sampled line density.

## One table, successive chunks

1. **Index the current chunk:** insert its surviving lines into the hashmap,
   marking repetitions within that chunk.
2. **Search later chunks:** mark matches against that table. Searches do not
   insert new entries; nonmatching lines remain for later rounds.
3. **Advance:** after all searches finish, clear the table and index the next
   chunk's surviving lines. Earlier chunks are finished and never searched again.

Each row below is one round: **purple** indexes, **amber** searches using that
table, and **gray** is finished. This shows chunks that fit in the table;
[bounded insertion](#bounded-insertion) handles denser chunks.

```mermaid
%%{init: {"flowchart": {"wrappingWidth": 240, "rankSpacing": 30, "nodeSpacing": 35}}}%%
flowchart TB
    subgraph R1["ROUND 1 · index A, then search B and C"]
        direction LR
        A1["Chunk A<br/>Build the hashmap"]
        B1["Chunk B<br/>Search using A's table"]
        C1["Chunk C<br/>Search using A's table"]
        A1 --> B1
        A1 --> C1
        B1 ~~~ C1
    end
    subgraph R2["ROUND 2 · index surviving B, then search C"]
        direction LR
        A2["Chunk A<br/>Finished; do not revisit"]
        B2["Chunk B<br/>Rebuild the hashmap"]
        C2["Chunk C<br/>Search using B's table"]
        A2 ~~~ B2
        B2 --> C2
    end
    subgraph R3["ROUND 3 · index surviving C"]
        direction LR
        A3["Chunk A<br/>Finished; do not revisit"]
        B3["Chunk B<br/>Finished; do not revisit"]
        C3["Chunk C<br/>Rebuild the hashmap"]
        A3 ~~~ B3 ~~~ C3
    end
    R1 -->|Clear and reuse the same hashmap| R2
    R2 -->|Clear and reuse the same hashmap| R3
    R3 --> O["Compact A, B and C in order<br/>Keep only unmarked, accepted lines"]
    style R1 fill:#f7f9fc,stroke:#8a98a8,color:#172d45
    style R2 fill:#f7f9fc,stroke:#8a98a8,color:#172d45
    style R3 fill:#f7f9fc,stroke:#8a98a8,color:#172d45
    classDef index fill:#f1ecff,stroke:#8064a2,color:#312147
    classDef search fill:#fff2dc,stroke:#b78030,color:#513710
    classDef done fill:#f2f3f5,stroke:#98a1ad,color:#59616c
    classDef output fill:#e7f5e8,stroke:#4d8653,color:#203d24
    class A1,B2,C3 index
    class B1,C1,C2 search
    class A2,A3,B3 done
    class O output
```

## Example

In the [shared example](../architecture.md#shared-example), A holds positions
1–2, B holds 3–4 and C holds 5:

| Round | Hashmap contents | New duplicate marks |
|---|---|---|
| Index A, search B and C | `alpha`, `beta` | Position 3 (`alpha`) and position 5 (`beta`) |
| Index surviving B, search C | `gamma` | None; C's only line is already marked |
| Index surviving C | Empty | None |

Compaction keeps positions 1, 2 and 4: `alpha`, `beta`, `gamma`. Marking later
occurrences leaves the first occurrence and its position intact until the
[shared compaction step](../architecture.md#working-file-and-output).

## Bounded insertion

Chunk sizing targets 50% table load. Insertion stops before a new entry would
exceed 70%; existing matches can still be marked.

If a chunk is denser than estimated, its indexed prefix is used to search the
unindexed remainder and later chunks. The table is then cleared and processing
resumes at the first unindexed line. Extra passes keep memory bounded and avoid
long probe chains.

## Parallel work

Indexing is serial; searches of later chunks can run concurrently against the
same read-only table. All searches finish before the table is reused. A single
whole-file chunk has no later chunks, so insertion remains serial.

## Cost and suitable workloads

With `n` chunks and no saturation, indexing and searches schedule up to
`n(n+1)/2` tasks. Marked lines and empty tables reduce useful work; saturation
can add tasks. This is a task count, not a timing prediction.

Avoiding partition writes helps with few chunks or cached long lines. Many
chunks require repeated scans, which become expensive when pages cannot stay
in RAM. See [engine selection](../engine-selection.md) for the tradeoff with
[disk partitions](disk-partitions.md).
