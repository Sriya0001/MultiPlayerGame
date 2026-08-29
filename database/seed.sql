-- =============================================================================
-- Initial Seed Data
-- =============================================================================

INSERT OR IGNORE INTO players (player_id, username, score, high_score, kills) VALUES
    (1,  'AlphaOne',   1200, 2500, 15),
    (2,  'ShadowBlade', 850, 1800,  9),
    (3,  'CyberKnight', 450,  900,  4),
    (10, 'Valkyrie',   2100, 3200, 22),
    (99, 'BotChampion', 500,  500,  5);
