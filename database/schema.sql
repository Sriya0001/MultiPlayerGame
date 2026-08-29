-- =============================================================================
-- Multiplayer Game Server Database Schema
-- Compatible with MySQL 8.0 and SQLite3
-- =============================================================================

-- 1. Players Table
CREATE TABLE IF NOT EXISTS players (
    player_id   INTEGER PRIMARY KEY,
    username    VARCHAR(64) NOT NULL DEFAULT 'player',
    score       INTEGER NOT NULL DEFAULT 0,
    high_score  INTEGER NOT NULL DEFAULT 0,
    kills       INTEGER NOT NULL DEFAULT 0,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_players_high_score ON players(high_score DESC);

-- 2. Matches Table
CREATE TABLE IF NOT EXISTS matches (
    match_id          VARCHAR(64) PRIMARY KEY,
    match_status      VARCHAR(32) NOT NULL DEFAULT 'WAITING',
    start_time        TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time          TIMESTAMP NULL,
    total_players     INTEGER DEFAULT 0,
    total_kills       INTEGER DEFAULT 0,
    winner_player_id  INTEGER NULL
);

CREATE INDEX IF NOT EXISTS idx_matches_start_time ON matches(start_time DESC);

-- 3. Game Events Table
CREATE TABLE IF NOT EXISTS game_events (
    event_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    match_id    VARCHAR(64) NOT NULL,
    player_id   INTEGER NOT NULL,
    event_type  VARCHAR(32) NOT NULL,
    event_data  TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_events_match ON game_events(match_id, created_at);
CREATE INDEX IF NOT EXISTS idx_events_player ON game_events(player_id);

-- 4. Load Test Benchmark Runs Table
CREATE TABLE IF NOT EXISTS test_runs (
    run_id               VARCHAR(64) PRIMARY KEY,
    test_name            VARCHAR(64) NOT NULL,
    concurrent_users     INTEGER NOT NULL,
    duration_seconds     INTEGER NOT NULL,
    total_requests       INTEGER NOT NULL,
    successful_requests  INTEGER NOT NULL,
    failed_requests      INTEGER NOT NULL,
    requests_per_second  REAL NOT NULL,
    avg_latency_ms       REAL NOT NULL,
    p50_latency_ms       REAL NOT NULL,
    p95_latency_ms       REAL NOT NULL,
    p99_latency_ms       REAL NOT NULL,
    max_latency_ms       REAL NOT NULL,
    error_rate_percent   REAL NOT NULL,
    created_at           TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_test_runs_date ON test_runs(created_at DESC);
