# Database Architecture & Relational Storage Design

## 1. Dual-Tier Storage Strategy

Multiplayer game backends require microsecond tick updates while demanding strict durability for user accounts, leaderboards, and audit logs. The platform implements a **Dual-Tier Storage Architecture**:

```
                       APPLICATION DATA PATH
                                 │
                 ┌───────────────┴───────────────┐
                 ▼                               ▼
       TIER 1: IN-MEMORY CACHE          TIER 2: RELATIONAL DB
        (Native RESP Redis)                  (MySQL 8.0)
 ┌──────────────────────────────┐ ┌──────────────────────────────┐
 │ • Real-time (x, y) Positions │ │ • Player Account & Scores    │
 │ • Active Session TTLs        │ │ • Match Lifecycle & Winners  │
 │ • Ephemeral Match State      │ │ • Historical Game Events     │
 │ • Read/Write Latency: ~0.1ms │ │ • Test Run Benchmark Records │
 └──────────────────────────────┘ └──────────────────────────────┘
```

---

## 2. Relational Schema & Entity-Relationship Model

```
 ┌──────────────────────┐         ┌──────────────────────┐
 │       players        │         │       matches        │
 ├──────────────────────┤         ├──────────────────────┤
 │ player_id (PK, INT)  │◄───┐    │ match_id (PK, VARCHAR│
 │ username (VARCHAR)   │    │    │ match_status (VARCHAR│
 │ score (INT)          │    │    │ start_time (TIMESTAMP│
 │ high_score (INT, IDX)│    │    │ end_time (TIMESTAMP) │
 │ kills (INT)          │    │    │ total_players (INT)  │
 │ created_at           │    │    │ total_kills (INT)    │
 │ updated_at           │    │    │ winner_player_id     │
 └──────────────────────┘    │    └──────────────────────┘
                             │
                             │    ┌──────────────────────┐
                             │    │     game_events      │
                             │    ├──────────────────────┤
                             │    │ event_id (PK, BIGINT)│
                             │    │ match_id (IDX, VARCH)│
                             └───-│ player_id (IDX, INT) │
                                  │ event_type (VARCHAR) │
                                  │ event_data (JSON/TXT)│
                                  │ created_at           │
                                  └──────────────────────┘
```

---

## 3. Database Indexing & Query Optimizations

| Table | Index | Type | Purpose |
|:---|:---|:---|:---|
| `players` | `idx_players_high_score` | B-Tree (`high_score DESC`) | Accelerates global leaderboard queries from $O(N)$ table scans to $O(\log N)$ index range scans. |
| `matches` | `idx_matches_start_time` | B-Tree (`start_time DESC`) | Optimizes match history lookups and recent match statistics. |
| `game_events` | `idx_events_match` | Composite B-Tree (`match_id, created_at`) | Fast sequential playback of game match events for anti-cheat and replay systems. |
| `test_runs` | `idx_test_runs_date` | B-Tree (`created_at DESC`) | Instant retrieval of benchmark performance trendlines over time. |

---

## 4. Parameterized Query Security & Prepared Statements

All SQL operations in `DatabaseManager` use compiled prepared statements (`MYSQL_STMT` and `mysql_stmt_bind_param`):

```cpp
// Example Parameterized Player Upsert
const char* sql = R"(
    INSERT INTO players (player_id, username, score, high_score, kills, updated_at)
    VALUES (?, ?, ?, ?, ?, NOW())
    ON DUPLICATE KEY UPDATE
        score = VALUES(score),
        high_score = GREATEST(high_score, VALUES(score)),
        kills = GREATEST(kills, VALUES(kills)),
        updated_at = NOW();
)";
```

- **Zero SQL Injection Risk**: Bound values are transmitted as typed binary parameters across the wire rather than concatenated strings.
- **Query Plan Reuse**: MySQL server compiles the AST and execution plan once, saving parse overhead on high-frequency transactions.
