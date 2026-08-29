-- =============================================================================
-- MySQL 8.0 Relational Database Schema
-- =============================================================================

USE game_server;

-- 1. Players Table
CREATE TABLE IF NOT EXISTS players (
    player_id   INT PRIMARY KEY,
    username    VARCHAR(64) NOT NULL DEFAULT 'player',
    score       INT NOT NULL DEFAULT 0,
    high_score  INT NOT NULL DEFAULT 0,
    kills       INT NOT NULL DEFAULT 0,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_players_high_score (high_score DESC)
) ENGINE=InnoDB;

-- 2. Matches Table
CREATE TABLE IF NOT EXISTS matches (
    match_id          VARCHAR(64) PRIMARY KEY,
    match_status      VARCHAR(32) NOT NULL DEFAULT 'WAITING',
    start_time        TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    end_time          TIMESTAMP NULL,
    total_players     INT DEFAULT 0,
    total_kills       INT DEFAULT 0,
    winner_player_id  INT NULL,
    INDEX idx_matches_start_time (start_time DESC)
) ENGINE=InnoDB;

-- 3. Game Events Table
CREATE TABLE IF NOT EXISTS game_events (
    event_id    BIGINT PRIMARY KEY AUTO_INCREMENT,
    match_id    VARCHAR(64) NOT NULL,
    player_id   INT NOT NULL,
    event_type  VARCHAR(32) NOT NULL,
    event_data  TEXT,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_events_match (match_id, created_at),
    INDEX idx_events_player (player_id)
) ENGINE=InnoDB;

-- 4. Load Test Runs Table
CREATE TABLE IF NOT EXISTS test_runs (
    run_id               VARCHAR(64) PRIMARY KEY,
    test_name            VARCHAR(64) NOT NULL,
    concurrent_users     INT NOT NULL,
    duration_seconds     INT NOT NULL,
    total_requests       INT NOT NULL,
    successful_requests  INT NOT NULL,
    failed_requests      INT NOT NULL,
    requests_per_second  DOUBLE NOT NULL,
    avg_latency_ms       DOUBLE NOT NULL,
    p50_latency_ms       DOUBLE NOT NULL,
    p95_latency_ms       DOUBLE NOT NULL,
    p99_latency_ms       DOUBLE NOT NULL,
    max_latency_ms       DOUBLE NOT NULL,
    error_rate_percent   DOUBLE NOT NULL,
    created_at           TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_test_runs_date (created_at DESC)
) ENGINE=InnoDB;
