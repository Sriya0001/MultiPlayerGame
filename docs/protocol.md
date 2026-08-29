# Game Server Wire Protocol

**Version:** 1.0  
**Transport:** TCP (IPv4)  
**Default port:** 7777

---

## Message Framing

The protocol uses **4-byte little-endian length-prefix framing**.

Every message (both request and response) is encoded as:

```
┌─────────────────────────────┬──────────────────────────────────────┐
│  Header (4 bytes, LE u32)   │  Payload (N bytes, UTF-8 JSON)       │
│  N = byte length of payload │                                       │
└─────────────────────────────┴──────────────────────────────────────┘
```

**Why not newlines?**  
TCP is a byte stream. Newline framing requires scanning the entire buffer
and breaks if the JSON payload ever contains a literal `\n`. Length-prefix
framing lets the receiver know exactly how many bytes to read before parsing.

**Max message size:** 1 MiB (1,048,576 bytes). The server closes the
connection if a message exceeds this limit.

---

## Connection Lifecycle

```
Client                          Server
  │                               │
  │──── TCP SYN ─────────────────►│
  │◄─── TCP SYN-ACK ──────────────│
  │                               │  (connection registered, ID assigned)
  │                               │
  │──── [len][JOIN request] ─────►│
  │◄─── [len][JOIN response] ─────│
  │                               │
  │──── [len][MOVE request] ─────►│
  │◄─── [len][MOVE response] ─────│
  │                               │
  │         ... (many messages)   │
  │                               │
  │──── [len][LEAVE request] ────►│
  │◄─── [len][LEAVE response] ────│
  │                               │
  │──── TCP FIN ─────────────────►│
  │                               │  (connection unregistered)
```

A client **stays connected** for the duration of a session and sends many
messages over the same TCP socket.

---

## Request Format

All requests are JSON objects with a mandatory `action` field:

```json
{
  "action": "<ACTION_NAME>",
  ... action-specific fields ...
}
```

---

## Actions

### JOIN
Player joins the game session.

**Request:**
```json
{ "action": "JOIN", "player_id": 101 }
```

**Response (success):**
```json
{ "action": "JOIN", "status": "ok" }
```

---

### MOVE
Player moves to a new position.

**Request:**
```json
{ "action": "MOVE", "player_id": 101, "x": 120, "y": 240 }
```

**Response:**
```json
{ "action": "MOVE", "status": "ok" }
```

---

### ATTACK
Player attacks another player.

**Request:**
```json
{ "action": "ATTACK", "player_id": 101, "target_id": 102 }
```

**Response:**
```json
{ "action": "ATTACK", "status": "ok" }
```

---

### CHAT
Player sends a chat message.

**Request:**
```json
{ "action": "CHAT", "player_id": 101, "message": "hello" }
```

**Response:**
```json
{ "action": "CHAT", "status": "ok" }
```

---

### GET_STATE
Player requests the current game state.

**Request:**
```json
{ "action": "GET_STATE", "player_id": 101 }
```

**Response:**
```json
{ "action": "GET_STATE", "status": "ok" }
```

*(Full game state payload added in Milestone 4.)*

---

### UPDATE_SCORE
Update a player's score.

**Request:**
```json
{ "action": "UPDATE_SCORE", "player_id": 101, "score": 500 }
```

**Response:**
```json
{ "action": "UPDATE_SCORE", "status": "ok" }
```

---

### LEAVE
Player leaves the game session.

**Request:**
```json
{ "action": "LEAVE", "player_id": 101 }
```

**Response:**
```json
{ "action": "LEAVE", "status": "ok" }
```

---

## Error Responses

When a request cannot be processed:

```json
{ "status": "error", "message": "<reason>" }
```

**Possible reasons:**

| Reason | Cause |
|---|---|
| `"invalid JSON"` | Request body is not valid JSON |
| `"missing action"` | `action` field absent or not a string |
| `"unknown action"` | `action` value not recognised |

---

## Implementation Notes

- Byte order: **little-endian** for the 4-byte length header.
- The payload is **UTF-8** encoded JSON with no BOM.
- JSON keys are case-sensitive.
- The server does not send unsolicited messages (no push/broadcast yet).
- Broadcast / push events will be added in a future milestone.
