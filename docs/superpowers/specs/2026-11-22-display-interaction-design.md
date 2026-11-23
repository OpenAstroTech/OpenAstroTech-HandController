# HandController Display & Interaction Design

Date: 2026-11-22
Status: approved in conversation, pending final review
Baseline: main at `fcca5ab` plus the uncommitted v0.3.1 integration (dispatch
layer, 54-item menu draft, idle screen first cut, pin `#v0.3.1`)

## Intent

The HandController (RP2040, SSD1306 128x64, one rotary encoder with push
button) is the only interface physically at the mount. It serves two modes
with one face each user never has to think about:

1. **Standalone** — just the user, the mount, and the HC. The HC must carry
   the session setup a GPS-less mount needs (hour angle, time, date, UTC
   offset) and the everyday actions (goto, park/unpark, home, tracking).
2. **Connected** — a miniPC rides the mount, the user is remoted indoors.
   The HC is the local override: jog the axes, home, park, unpark,
   start/stop tracking, autohome RA/DEC, stop everything.

The display has two states: an **idle screen** that mirrors the mount's own
InfoDisplay as closely as the LX200 protocol reaches, and a **menu** that
opens on a short press. Long-running commands close the menu so the idle
screen can report what the mount is doing.

## Goals

- Idle by default, looking like the mount's InfoDisplay (`SSD1306_128x64_Display.cpp`).
- Short press opens the menu; it reopens at the same location (the
  navigator already preserves state across close).
- Long-press on idle = **Stop Everything** (`:Q#`, tracking included),
  immediate, no confirmation — the panic gesture.
- A **jog mode** for nudging axes with the dial as a speed throttle.
- Menu items that depend on mount hardware appear only when the mount has
  that hardware (from `:XGM#`).
- Destructive or walk-away actions (Park, Home, Autohome) get a Yes/No
  confirmation screen; slews and Stop-all do not.
- Commands that start a motion or end a session (slew, park, unpark, home,
  autohome, HA store, stop-all) close the menu back to the idle screen.
- Standalone session setup in the menu: HA set, local time, date, UTC offset.

## Non-goals (for now)

- Slew progress bars/percentages. The idle screen shows the state string
  ("Slewing…" etc.) and moving-axis letters; that is enough.
- Calibration editing from the HC (steps/deg, tracking speed, backlash).
  Calibration happens in daylight with a PC. The whole Cal submenu is gone.
- An INFO submenu — the idle screen is the info display.
- Distinguishing the two modes in software. The menu is the same; standalone
  merely means no client is talking.

## Anti-goals

- Sending blind values to the mount: any numeric editor the HC shows must
  either be composing a fresh value the user just chose (target coords, HA,
  time) or be preloaded from the mount first. This is why Cal died: its
  value items edit persisted state that the menu cannot preload today.
- A second button's worth of gestures. The vocabulary is exactly: rotate,
  click, long-press.

## Approaches considered

- **Jog axis selection** — two menu entries (simplest, extra click), one
  screen where click toggles the axis when stopped and stops both while
  moving (**picked**), or press-and-turn for the second axis (fast but
  invisible until taught).
- **Hardware gating** — a grammar field in menu.txt vs an HC-side check
  table (**picked**, see Hardware gating below: exact-token logic and no
  library release).
- **Confirm-screen cancel** — HC-side release gating so long-press cancels
  (machinery, changed feel) vs accepting that long-press is not cancel on
  confirm screens and defaulting the cursor to "No" (**picked** — a
  reflexive press is then always safe, no machinery, no library change).
- **Jog/Stop-all latency** — a second writer bypassing the job queue vs
  keeping the single-writer pipeline (**picked** the pipeline: NoReply
  commands still wait for the in-flight job, but the realistic bound is
  ~300 ms and the normal case is tens of ms; a bypass serves the
  pathological 5% at the cost of the pipeline's core invariant).

## Constraints

- Hardware: one encoder with one push button. Vocabulary: rotate, click,
  long-press; short click = confirm, long-press = cancel **inside edit
  modes** (that is where the library implements it). On confirm screens a
  click fires on button-down, so holding the button does what a click
  does — hence the cursor defaults to "No" there (see Confirmations).
- Library: OledMenu v0.3.1 as-is. No library changes are needed for this
  design; if that changes, the change goes through the normal
  tag-and-pin-bump cycle.
- Protocol: LX200 as implemented by OpenAstroTracker-Firmware (handlers in
  `src/core/meade/MeadeParser*.cpp` are authoritative over the docs). All
  reply types are already classified in `src/MessageJob.cpp`.
- Pipeline: exactly one job in flight. NoReply commands complete as soon
  as they are written, but they still wait for the current job to finish:
  while the jog screen is open the HC's own `:GX#` polls pause, so jog and
  Stop-all commands only ever queue behind client traffic (~300 ms worst
  realistic case, 2 s pathological, tens of ms normally). No second
  writer to the mount serial.
- Bench mount (reference hardware): `Mega,NEMA|16|3600,NEMA|16|3600,NO_GPS,
  AUTO_AZ_ALT,NO_GYRO,NO_LCD,INFO_I2C_SSD1306_128x64,NO_FOC,HSAH,HSAV,
  ENDSW_DEC` — so it has AZ/ALT steppers and Hall auto-homing, no GPS, no
  gyro, no focuser.

## The idle screen

Already implemented in first-cut form; keep and extend:

- Stepper-state boxes RA / DEC / ALT / AZ / TRK / FOC (filled = running),
  from the six activity letters of the `:GX#` reply.
- Status string + bouncing pixel (state string, uppercased).
- Current RA and DEC, formatted from the compact `:GX#` fields
  (HHMMSS → HH:MM:SS, ±DDMMSS → ±DD*MM:SS).
- Bottom strip: mount link (`MOUNT` / `----`) and HC version.
- Refreshed by `StatusRefresher` (1 Hz standalone, piggy-backed at 2 s when
  a client is active). The menu pauses idle rendering while open; the
  refresher keeps caching `:GX#` either way.
- While a long command runs (slew, park, home, autohome), nothing special
  happens on screen — the state string and activity letters tell the story.

## The menu

Trimmed from the 54-item firmware mirror to what the two modes need:

```
HandController
  Tracking [ON/OFF]        :MT1# / :MT0#; state synced from the :GX# cache
  Jog                      opens the jog screen (UI-only item, like Exit)
  Goto
    Object (21 POIs)       :Sr/:Sd/:MS composite, then close menu
    Home                   confirm -> :hF#, close
    Park                   confirm -> :hP#, close
    Unpark                 :hU#, close
    Target
      RA h/m/s, DEC sign/d/m/s (IntegerRanged/Toggle, as built)
      Slew                 :Sr/:Sd/:MS composite, then close
  Align
    Autohome RA    [HSAH]  confirm -> :MHR search, close
    Autohome DEC   [HSAV]  confirm -> :MHD search, close
    Set home here          :SHP#
    AZ +1' [AZ]  AZ -1'    :MAZ±1.0#
    ALT +1' [ALT] ALT -1'  :MAL±1.0#
  Setup
    HA hours / minutes
    Store & track          :SHHH:MM# then :MT1#, close
    Date month/day/year
    Store date             :SCMM/DD/YY#
    Time hours/minutes/seconds
    Store time             :SLHH:MM:SS#
    UTC offset
    Store UTC              :SG±n#
  Focus            [FOC]
    In (:F+#)  Out (:F-#)  Stop (:FQ#)  Rate 1-4 (:F1#..:F4#)
  Stop all                :Q#, close (same command as long-press on idle)
  Exit                    close menu
```

`[TOKEN]` marks the hardware gate (below). Stores for date/time/UTC stay in
the menu (they're settings, not motions); HA store closes because it starts
tracking and the idle screen saying "Tracking" is the feedback.

## Confirmations

Park, Home, Autohome RA and Autohome DEC each get a Yes/No screen, built as
declarative submenus in the menu definition (no library dialog primitive
exists; submenus are free):

```
> Park               (click)          > Park mount?
  Home                                Are you sure?
  ...                                 > Yes
                                        No
```

Yes fires the command and closes the menu; No steps back (or "< Back").
The cursor always lands on "No": a click fires on button-down in
navigation mode, so a reflexive press — including a held one — must be
safe. Choosing "Yes" is a deliberate second click. Slews stay unconfirmed
(choosing the object is the intent). Stop-all is never confirmed — speed
is its point.

## Jog mode

A dedicated full-screen mode reached from the menu's Jog item; long-press
exits back to the menu.

```
JOG  RA
>RA   +0.40 deg/s        <- highlighted axis is what the dial moves
 DEC   0.00 deg/s
```

- Rotate: throttle the highlighted axis through a speed ladder, through
  zero into the reverse direction. Ladder: ±0.02, 0.05, 0.1, 0.25, 0.5,
  1.0, 2.0 °/s (builder may tune rungs; first rungs must be slow enough for
  framing, last ones fast enough to cross the sky).
- Click while either axis is moving: both axes to zero speed.
- Click while stopped: switch the highlighted axis (RA <-> DEC).
- Long-press: stop both axes, exit jog.

Transport: `:XSM1#` on entry (manual slew mode), `:XSXn.n#` / `:XSYn.n#`
set RA/DEC speed immediately and take no reply, both speeds to 0 and
`:XSM0#` on exit. Speed changes are sent live as the dial moves.

Open implementation question for the builder to verify on hardware: whether
tracking resumes by itself after `:XSM0#`; if not, follow it with `:MT1#`
on exit.

## Hardware gating

When the mount connects, the HC sends one `:XGM#` (FromController job,
like the `:GX#` polls) and caches the reply. The menu definition is built
from a def string filtered by an HC-side `id -> check` table:

| Menu part | Requires (`:XGM#` comma fields) |
|---|---|
| Focus submenu | `FOC` (exact field; `NO_FOC` means absent) |
| Autohome RA | `HSAH` |
| Autohome DEC | `HSAV` |
| AZ ±1' items | `AUTO_AZ` or `AUTO_AZ_ALT` |
| ALT ±1' items | `AUTO_ALT` or `AUTO_AZ_ALT` |

Matching is exact per comma-separated field — substring matching breaks on
`FOC` vs `NO_FOC`. Until an `:XGM#` reply exists the full menu is built;
the swap happens at connect time, when nobody is in the menu. The id-to-
token mapping is documented in `OledMenu/sim/menu.txt` comments so the
draft and the firmware stay honest; the sim always shows everything.

Rejected alternative: carrying the hardware string as a grammar field in
menu.txt (`Submenu, 50, "Focus", "FOC"`). It makes menu.txt the single
source but needs another library release and cannot express the two-token
AZ/ALT cases or the NO_FOC trap in a plain string. Revisit if the HC table
ever feels duplicative.

## Behavior notes

- "Close the menu" = `setActive(false)` immediately when the activity
  fires (not when the command completes); the idle screen picks up the
  resulting state.
- Reopening preserves the navigator's position (existing behavior —
  `setActive` only gates input and rendering).
- Tracking toggle: a BooleanMenuItem; its ON/OFF is synced from the
  activity letter T in the cached `:GX#` when fresh data arrives and the
  menu is open.
- While the jog screen is open, `StatusRefresher` pauses its own `:GX#`
  polls (the jog screen doesn't need them, and it frees the pipeline for
  jog commands; the idle screen's data goes stale for a moment and
  refreshes on exit).
- All new commands are already reply-classified in `MessageJob.cpp`
  (`:XSM/:XSX/:XSY` NoReply, `:SC` DoubleString, `:SL/:SG/:SH` NumberReply,
  `:MHR/:MHD` NumberReply).
- The uncommitted integration (dispatch layer, POI table, compose helpers,
  idle screen, 54-item def) is the baseline this design trims and extends;
  the dispatch tests and compose helpers survive as-is. The Cal value
  helper (`menuValueToCommand`) is reused by Setup's UTC offset.

## What is left to the builder

- Speed ladder rungs (within the stated intent).
- Autohome search direction letter (`:MHRr#` vs `:MHRl#`, `:MHDu#` vs
  `:MHDd#`): pick a sensible default, verify against the firmware handler.
- Confirm-screen wording and layout (one line of prompt, Yes/No).
- The exact jog screen layout within 128x64 and its render cadence.
- Menu id re-numbering to match the trimmed tree (and test updates).
- Whether the `:XGM#` query also runs periodically or only at connect
  (recommend: only at connect).
- Autohome direction: the letter is mandatory (`:MHR#` without one is
  rejected by the mount) — pick the default and verify against the
  firmware handler.
- TDD throughout, per repo convention: native tests for the new logic
  (gating table, jog throttle state machine, close-after-action dispatch)
  before the main.cpp wiring; hardware verification where possible.
