# Task: player names, score, and a moves log (HUD around the board)

## Reference

The person shared a screenshot of the target look: a wide gray canvas with
the board centered. Top bar: `Name: Black   Score: 0`. Bottom bar:
`Name: White   Score: 0`. Left panel: header `Black`, a two-column table
(`Time` / `Move`). Right panel: header `White`, same table. File letters
(a-h) above and below the board, rank numbers (1-8) on both sides of it,
next to the board itself (not the outer canvas edges). This matches the
"Additional Requirements" slide from the original course brief (moves log,
score = "cost" of captured pieces, player names).

**Explicit constraint from the person: don't change any move/legality
logic.** Everything here is either (a) new data derived from events that
already exist, or (b) a new rendering layer drawn around the existing
board. If implementing this seems to require touching `RuleEngine`,
`MovementRules`, or `isMovementLegal`, stop and ask - that would mean a
misunderstanding of scope, not a necessary part of this task.

---

## Data additions (all additive, zero behavior change to existing checks)

**`CaptureEvent`** (`RealTimeArbiter.hpp`) gains `PieceKind capturedKind`,
set from `occupant->getKind()` right where the event is already
constructed in `resolveArrival` (`src/real_time_arbiter/RealTimeArbiter.cpp`,
around the `events.push_back({pm.to, occupant->getColor(),
occupant->isKing()})` line) - `occupant` is already in scope there, this is
one more field, not new logic.

**Score**: standard piece values - pawn 1, knight 3, bishop 3, rook 5,
queen 9 (king isn't captured in the scoring sense - a king capture already
ends the game via `wasKing`/`gameOver`, don't add it to score). `GameEngine`
tracks `whiteScore`/`blackScore`, incremented in `applyCaptureEvents` using
a small kind→value lookup. Whoever's color did the capturing gets the
value of what they captured (i.e. white captures a black knight → white's
score +3).

**Player names**: `GameEngine::setPlayerNames(whiteName, blackName)` +
getters. Default to `"White"`/`"Black"` if never set (matches the
screenshot's own placeholder values).

**Move history**: a `MoveRecord { long long atMs; char color; std::string
notation; }` list, appended once per move/jump **when it's scheduled**
(not when it lands - the screenshot's `Time` column reads like elapsed
game-clock at the moment the move was made, e.g. `00:04.105`), so hook
this into `GameEngine::select()`/`jump()` right where `arbiter.scheduleMove`/
`scheduleJump` are already called - not into arrival resolution.

- **Notation - start simple, confirm before going further:** plain
  from-square/to-square (e.g. `e2e4`) is enough for v1 and doesn't need
  disambiguation logic. Full SAN (piece letters, `x` for captures, `+`/`#`,
  castling as `O-O`) is a reasonable stretch goal matching the screenshot
  in the earlier course slide more closely, but it's real added complexity
  (disambiguating which of two knights moved, detecting check, etc.) -
  don't build it without checking scope with the person first.

**Expose all of the above through `GameSnapshot`**: `whiteName`,
`blackName`, `whiteScore`, `blackScore`, and the move history split by
color (e.g. `whiteMoves` / `blackMoves`, matching the screenshot's two
separate side tables) - or one combined list the view filters by color,
whichever reads cleaner in the existing snapshot style.

---

## Rendering: a HUD layer around the existing board, not inside `BoardView`

Keep `BoardView::render()` exactly as it is (still just the board + pieces
+ highlight + capture flashes, unchanged). Add a new piece that composes a
bigger canvas around that, so the board-only renderer stays testable in
isolation:

- Canvas size = board size + fixed margins: left/right panels (~200px
  each, per the screenshot's proportions), top/bottom bars (~40px each).
- Draw the board's own render inset at `(leftPanelWidth, topBarHeight)`.
- Top bar centered text: `Name: {blackName}   Score: {blackScore}` (black
  is the top side given the current board orientation - row 0 = black
  back rank). Bottom bar: same for white.
- Left panel: header text `Black`, then the `Time`/`Move` two-column table
  for black's moves. Right panel: same for white.
- File letters (a-h) drawn just above and below the board itself (not the
  outer canvas edges), rank numbers (1-8) just left and right of the board.
  These are static labels, not per-frame data - fine to draw once per frame
  from fixed cell math (`col * cellSize + cellSize/2` etc.), same as
  everything else.
- Everything through `Img` - `put_text` for all text, `rectangle` (already
  supports filled via negative thickness) for panel backgrounds/borders. No
  new `Img` primitives should be needed for this task; say so if one
  genuinely is.

**Open question to confirm, don't guess:** the move list will eventually
overflow a fixed-height panel. Simplest v1: show only the most recent N
rows that fit (older ones scroll off the top, newest at the bottom) rather
than building actual scroll/pagination. Confirm this is acceptable before
building anything more elaborate.

---

## Acceptance criteria

- [ ] `RuleEngine`/`MovementRules`/`isMovementLegal` are untouched - `git
      diff` those files should be empty when this task is done.
- [ ] Capturing a piece updates the capturing side's score by the correct
      value; capturing a king still ends the game exactly as before (score
      change on that final capture is fine either way, confirm which you
      picked).
- [ ] Player names default sensibly and can be set before the game starts.
- [ ] Every scheduled move/jump appears in the correct side's log with a
      plausible elapsed-time stamp, in order.
- [ ] The rendered frame visually matches the reference screenshot's
      layout (panels, bars, coordinate labels) - compare a headless PNG
      render against it side by side, don't just eyeball your own output.
- [ ] `BoardView::render()` itself is unchanged (diff it) - the HUD is a
      separate layer that calls it, not a modification of it.
