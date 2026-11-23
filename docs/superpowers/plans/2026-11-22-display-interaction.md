# Display & Interaction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rework the HandController UI per the approved design: trimmed menu with confirm screens and hardware gating, jog mode with the dial as a speed throttle, long-press Stop-all, and the pipeline plumbing for `:XGM#`.

**Architecture:** Pure logic (id tables, menu-definition builder, gating checks, jog state machine) lives in host-testable headers; `main.cpp` keeps only display/encoder/pipeline wiring that cannot run native. The single-writer job pipeline is kept; jog commands are ordinary NoReply jobs.

**Tech Stack:** PlatformIO, C++ (Arduino), OledMenu v0.3.1 (no library changes), Unity native tests (`pio test -e native`).

**Spec:** `docs/superpowers/specs/2026-11-22-display-interaction-design.md`

## Global Constraints

- One encoder button: rotate / click / long-press. Long-press = cancel only inside edit modes; confirm screens fire on button-down, so their cursor defaults to "No" (spec, Confirmations).
- Never send a blind persisted value (spec, Anti-goals). All `:S...` set-commands compose from values the user just edited on-device.
- No second writer to the mount serial (spec, Constraints). While jog is active, `StatusRefresher` own-polls pause.
- Reply types: `MessageJob.cpp` already classifies every command used here — verified in the spec's Behavior notes. No table changes expected.
- Confirm "Yes" fires `menuIdToCommand(id)` and closes the menu; "No" calls `menuNav->returnToParent()` (public in v0.3.1).
- Speed ladder: ±0.02, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0 deg/s (spec: first rungs framing-slow, last fast; builder may tune within intent).
- Menu id numbering is normative — taken from the tree in Task 1; `sim/menu.txt` gains the same tree in comments (Task 4).

## Review Focus

- **Long-press on jog exit must not double as a click** — exiting by holding 750 ms must stop both axes and exit once; the release afterward must not toggle the axis or fire a menu click. Test: `test_jog_controller` `test_long_press_exits_once_and_consumes_the_press` (Task 3).
- **`NO_FOC` must not satisfy a `FOC` gate** — gating matches whole comma fields. Test: `test_menu_definition` `test_no_foc_does_not_enable_focus` (Task 2).
- **Yes on a confirm screen fires exactly one command** — button-down edge, one activity, one enqueue; a held press fires the same single command. Test: `test_menu_dispatch` `test_yes_ids_map_to_commands_and_close` (Task 1).
- **A mount swap must re-query `:XGM#`** — the profile resets on each USB mount callback, so a different mount's menu appears after reconnect. Verified on hardware (Task 5; no native path — state lives in `main.cpp`).
- **Time/date composition matches the mount parser exactly** — `:SLHH:MM:SS#` (2-digit fields, colon separated) and `:SCMM/DD/YY#` (slashes); a wrong separator silently fails on the mount with "0". Tests in `test_menu_dispatch` (Task 1).

---

### Task 1: MenuCommands — trimmed tree, composes, close-after-action

**Files:**
- Modify: `src/MenuCommands.hpp`
- Test: `test/test_menu_dispatch/main.cpp` (rewrite)

**Interfaces:**
- Consumes: nothing new (existing `SkyCoordinate`, `parseMenuPayload`).
- Produces (Task 2/4/5 rely on these exact names):
  - `enum MenuCommandId` with exactly: `ID_MENU_ROOT=1, ID_TRACKING=2, ID_JOG=3, ID_SUB_GOTO=10, ID_GOTO_OBJECT=100, ID_GOTO_HOME_Q=110, ID_GOTO_HOME_NO=111, ID_GOTO_HOME_YES=112, ID_GOTO_PARK_Q=120, ID_GOTO_PARK_NO=121, ID_GOTO_PARK_YES=122, ID_GOTO_UNPARK=103, ID_SUB_TARGET=20, ID_TARGET_RA_HOURS=200, ID_TARGET_RA_MINUTES=201, ID_TARGET_RA_SECONDS=202, ID_TARGET_DEC_SIGN=203, ID_TARGET_DEC_DEGREES=204, ID_TARGET_DEC_MINUTES=205, ID_TARGET_DEC_SECONDS=206, ID_TARGET_SLEW=207, ID_SUB_ALIGN=30, ID_ALIGN_AUTOHOME_RA_Q=310, ID_ALIGN_AUTOHOME_RA_NO=311, ID_ALIGN_AUTOHOME_RA_YES=312, ID_ALIGN_AUTOHOME_DEC_Q=320, ID_ALIGN_AUTOHOME_DEC_NO=321, ID_ALIGN_AUTOHOME_DEC_YES=322, ID_ALIGN_SET_HOME=330, ID_ALIGN_AZ_PLUS=340, ID_ALIGN_AZ_MINUS=341, ID_ALIGN_ALT_PLUS=350, ID_ALIGN_ALT_MINUS=351, ID_SUB_SETUP=40, ID_SETUP_HA_HOURS=402, ID_SETUP_HA_MINUTES=403, ID_SETUP_HA_STORE=404, ID_SETUP_DATE_MONTH=410, ID_SETUP_DATE_DAY=411, ID_SETUP_DATE_YEAR=412, ID_SETUP_DATE_STORE=413, ID_SETUP_TIME_HOURS=420, ID_SETUP_TIME_MINUTES=421, ID_SETUP_TIME_SECONDS=422, ID_SETUP_TIME_STORE=423, ID_SETUP_UTC=430, ID_SETUP_UTC_STORE=431, ID_SUB_FOCUS=50, ID_FOCUS_IN=500, ID_FOCUS_OUT=501, ID_FOCUS_STOP=502, ID_FOCUS_RATE=503, ID_STOP_ALL=90, ID_MENU_EXIT=99`
  - `const char* menuIdToCommand(uint16_t id)` — `112":"hF#", 122":hP#", 103":hU#", 312":MHRr#", 322":MHDd#", 330":SHP#", 340":MAZ+1.0#", 341":MAZ-1.0#", 350":MAL+1.0#", 351":MAL-1.0#", 500":F+#", 501":F-#", 502":FQ#", 90":Q#"`; nullptr otherwise (containers, editors, composites, No-items, Jog, Exit).
  - `const char* trackingToggleToCommand(const char* onOff)` — `"ON"→":MT1#"`, `"OFF"→":MT0#"`, else nullptr.
  - `const char* rateOptionToCommand(...)`, `const char* focusRateOptionToCommand(...)` — unchanged behavior.
  - `struct SkyCoordinate`, `poiCoordinates(name, out)` — unchanged (21 entries).
  - `struct TargetState { SkyCoordinate target; }` (HA moves out), `initTargetState`, `updateTargetState` (only the 200-206 ids now), `targetCoordinate`, `composeSrCommand`, `composeSdCommand` — unchanged behavior minus HA.
  - `struct SetupState { uint8_t haHours, haMinutes; uint8_t dateMonth, dateDay, dateYear; uint8_t timeHours, timeMinutes, timeSeconds; int utcOffset; }`, `initSetupState`, `bool updateSetupState(SetupState*, uint16_t id, const char* value)` — true for ids 402/403/410/411/412/420/421/422/430 (atol; dateYear stored as 2-digit 0-99), false otherwise.
  - `void composeShCommand(const SetupState*, char*, size_t)` → `":SH%02u:%02u#"` (no sign — the mount's parser takes bare `HH<sep>MM`); `composeSlCommand(const SetupState*, char*, size_t)` → `":SL%02u:%02u:%02u#"`; `composeScCommand(const SetupState*, char*, size_t)` → `":SC%02u/%02u/%02u#"`; `composeSgCommand(const SetupState*, char*, size_t)` → `":SG%c%u#"` with `-` when utcOffset < 0 else `+`.
  - `bool menuActionClosesMenu(uint16_t id)` — true exactly for `ID_GOTO_OBJECT, ID_TARGET_SLEW, ID_GOTO_HOME_YES, ID_GOTO_PARK_YES, ID_GOTO_UNPARK, ID_ALIGN_AUTOHOME_RA_YES, ID_ALIGN_AUTOHOME_DEC_YES, ID_SETUP_HA_STORE, ID_STOP_ALL`.
  - Removed: `TargetState.haHours/haMinutes`, `updateTargetState` HA cases, `composeShCommand(const TargetState*)`, the whole Cal value API (`menuValueToCommand`, `ID_CAL_*`), old ids 11-15/21/30-32/41/300-306/400-414/600-602/700-706.

- [ ] **Step 1: Rewrite `test/test_menu_dispatch/main.cpp`** — keep the existing suite's shape; new cases: direct-command table above (one TEST_ASSERT per id, `menuActionClosesMenu` true/false sets, `trackingToggleToCommand` ON/OFF/nullptr, `updateSetupState` consumes each 402-431 id and rejects 200/999, composes: `composeShCommand` `":SH07:30#"`, `composeSlCommand` `":SL23:59:59#"`, `composeScCommand` `":SC12/31/26#"`, `composeSgCommand` `":SG+2#"`/`":SG-5#"`, POI lookups unchanged, Target compose unchanged, `parseMenuPayload` unchanged).
- [ ] **Step 2: Run `pio test -e native -f test_menu_dispatch` — expect FAIL** (new ids/functions missing, old ones present).
- [ ] **Step 3: Rewrite `src/MenuCommands.hpp`** to the interface above (comment each command with its mount-handler basis as today; note `:MHRr#`/`:MHDd#` direction letters are the builder-tunable defaults and mandatory — a bare `:MHR#` is rejected by the mount).
- [ ] **Step 4: Run `pio test -e native` — all suites PASS** (job-pipeline/command-types suites must be untouched-green).
- [ ] **Step 5: Commit** `git add src/MenuCommands.hpp test/test_menu_dispatch/main.cpp && git commit -m "Rework MenuCommands for the trimmed interaction tree"`

### Task 2: MenuDefinition — gated def builder

**Files:**
- Create: `src/MenuDefinition.hpp`
- Test: `test/test_menu_definition/main.cpp` (new suite)

**Interfaces:**
- Consumes: nothing from Task 1 except the id constants (inside the def string).
- Produces: `bool xgmHasField(const char* xgm, const char* field)` — exact match against one comma-separated field of the `:XGM#` reply (`#` stripped by caller; nullptr/empty xgm matches nothing); `String buildMenuDef(const char* xgm)` — the full def when xgm is nullptr/empty, otherwise omits: the Focus submenu unless `xgmHasField(xgm,"FOC")`, Autohome RA block unless `"HSAH"`, Autohome DEC block unless `"HSAV"`, the two AZ items unless `"AUTO_AZ"` or `"AUTO_AZ_ALT"`, the two ALT items unless `"AUTO_ALT"` or `"AUTO_AZ_ALT"`. The def text is the trimmed tree of Task 1's ids with four confirm submenus (`Submenu, 110, "Home mount?"` > `Action, 111, "No"` > `Action, 112, "Yes"`, same for 120/310/320), Target nested inside Goto, all items/editors from the spec's tree (IntegerRanged ranges verbatim: RA h 0-23, m/s 0-59, DEC d 0-89; date M/D/Y as Integer items with no range, month 1-12… ranges are the builder's call — use `IntegerRanged, 410, "Month", 1, 12`, `"Day", 1, 31`, `"Year", 0, 99`, time H/M/S 0-23/59/59, UTC `Integer, 430, "UTC offset"`).

- [ ] **Step 1: Write `test/test_menu_definition/main.cpp`** — cases: `test_null_or_empty_xgm_builds_everything` (def contains `"Focus"`, `"Autohome RA"`, `"AZ +1 arcmin"`); `test_xgm_fields_are_matched_exactly` (`xgmHasField("Mega,...,NO_FOC,...","FOC")` false, `("...FOC,...","FOC")` true, `"HSAH"` among fields true); `test_no_foc_does_not_enable_focus` (bench reply string from the spec's Constraints verbatim → def lacks `"Focus"` and `"FOC"`); `test_hall_sensors_gate_autohome` (bench reply → no `"Autohome RA"`/`"Autohome DEC"`); `test_az_alt_gate_nudges` (bench reply has `AUTO_AZ_ALT` → AZ and ALT items present; a reply with `AUTO_AZ` only → AZ yes, ALT no); `test_def_parses` (structural assertions only — OledMenu headers are not on the native include path: starts `"Menu, 1, "`, contains all four Yes/No id pairs, eleven `Submenu,` lines). Also `test_confirm_cursors_land_on_no` (in each confirm submenu the "No" Action line precedes the "Yes" line).
- [ ] **Step 2: Run `pio test -e native -f test_menu_definition` — expect FAIL** (header missing).
- [ ] **Step 3: Implement `src/MenuDefinition.hpp`** — `xgmHasField` walks comma fields; `buildMenuDef` is a `String def = F(...)` composition with the gated blocks behind `if (...) def += F(...)`. Keep item text identical to the spec tree so the sim diff stays mechanical.
- [ ] **Step 4: Run `pio test -e native` — all PASS.**
- [ ] **Step 5: Commit** `git add src/MenuDefinition.hpp test/test_menu_definition/main.cpp && git commit -m "Add hardware-gated menu definition builder"`

### Task 3: JogController — the throttle state machine

**Files:**
- Create: `src/JogController.hpp`
- Test: `test/test_jog_controller/main.cpp` (new suite)

**Interfaces:**
- Consumes: nothing (deliberately no OledMenu includes — the native env has no library on its include path).
- Produces:
  - `class JogInput { virtual bool upPulse()=0; virtual bool downPulse()=0; virtual bool selectDown()=0; virtual bool selectUp()=0; virtual unsigned long selectHeldMs(unsigned long now)=0; }` — pulses are consumed (true exactly once per detent).
  - `class JogController`: `void begin()`; `void setActive(bool)` (activation emits nothing — the caller sends `:XSM1#`); `bool isActive() const`; `const char* tick(JogInput*, unsigned long now)` — returns one queued command per call, nullptr when drained; `char activeAxis() const` (`'R'`/`'D'`); `float raSpeed() const; float decSpeed() const; bool isMoving() const`.
  - State: per-axis signed rung index −7..+7 (0 = stopped; sign = direction; speed = `SPEED_LADDER[abs(rung)]` × sign, ladder `{0.02, 0.05, 0.1, 0.25, 0.5, 1.0, 2.0}`).
  - Command forms: `":XSX0.050#"`, `":XSX-0.050#"`, `":XSX0.000#"` (and `XSY`), exit appends `":XSM0#"`.
  - Behavior per spec: up/down pulse moves the active axis one rung (clamped ±7) and emits its speed; click (press + release < 750 ms) while either axis moving → both rungs 0, emits both zeros; click while stopped → toggle active axis, emits nothing; held ≥ 750 ms once → both zeros + `":XSM0#"`, `isActive()` false, and the press is consumed (`selectUp`/held state reset so the release produces nothing).

- [ ] **Step 1: Write `test/test_jog_controller/main.cpp`** — a fake `JogInput` with scriptable pulses/holds. Cases: `test_dial_throttles_active_axis_through_ladder` (3 up-pulses from 0 → `":XSX0.100#"` ticked out one per call; `raSpeed()==0.1f`); `test_down_pulse_crosses_zero_into_reverse` (up once, down twice → `":XSX-0.020#"`); `test_click_while_moving_stops_both` (RA at rung 2, DEC at rung 1, press+release → ticks yield `":XSX0.000#"`, `":XSY0.000#"`, then nullptr); `test_click_while_stopped_toggles_axis` (press+release, no movement → no commands, `activeAxis()=='D'`); `test_long_press_exits_once_and_consumes_the_press` (hold 800 ms → `":XSX0.000#"`,`":XSY0.000#"`,`":XSM0#"`, `isActive()==false`, further `selectUp()` + ticks yield nullptr); `test_rungs_clamp_at_ladder_ends` (10 up-pulses → speed 2.0); `test_inactive_tick_yields_nullptr`.
- [ ] **Step 2: Run `pio test -e native -f test_jog_controller` — expect FAIL.**
- [ ] **Step 3: Implement `src/JogController.hpp`** — small pending-command queue (`const char* _pending[4]`, filled by the handlers, popped one per tick); edge-detect select via `_wasDown` + `selectHeldMs(now) >= 750`; on long-press set `_pressConsumed` so the release edge is ignored.
- [ ] **Step 4: Run `pio test -e native` — all PASS.**
- [ ] **Step 5: Commit** `git add src/JogController.hpp test/test_jog_controller/main.cpp && git commit -m "Add jog throttle state machine"`

### Task 4: Pipeline plumbing — cache the `:XGM#` reply

**Files:**
- Modify: `src/Controller.hpp` / `src/Controller.cpp` — `setLastXGM(String)`, `String getLastXGM() const`, backed like `_lastGX` (timestamp not needed).
- Modify: `src/JobPipeline.hpp` — route a `FromController` `:XGM#` reply into `setLastXGM` exactly where the `:GX#` reply is routed into `setLastGX` (both sources).
- Test: `test/test_job_pipeline/main.cpp` (extend)

**Interfaces:**
- Produces: `controller->getLastXGM()` — the raw reply string (with trailing `#`), empty until first reply. Task 5 reads it on each loop to decide whether to rebuild the menu.

- [ ] **Step 1: Extend `test/test_job_pipeline/main.cpp`** — mirror the existing GX-caching test: run a `FromController` `:XGM#` job whose fake mount answers `"Mega,...,NO_FOC,...,#"`; assert `controller->getLastXGM()` equals it, and that a client `:XGM#` pass-through caches it too.
- [ ] **Step 2: Run `pio test -e native -f test_job_pipeline` — expect FAIL** (no `getLastXGM`).
- [ ] **Step 3: Implement** the Controller fields + the one-line pipeline hook (copy the GX line, `startsWith(":XGM")` guard on the job's command).
- [ ] **Step 4: Run `pio test -e native` — all PASS.**
- [ ] **Step 5: Commit** `git add src/Controller.hpp src/Controller.cpp src/JobPipeline.hpp test/test_job_pipeline/main.cpp && git commit -m "Cache the :XGM# hardware profile in the Controller"`

### Task 5: main.cpp wiring, docs, hardware verification

**Files:**
- Modify: `src/main.cpp` (INFO_DISPLAY block)
- Modify: `src/MenuCommands.hpp` — no (Task 1 done); **Modify: `OledMenu/sim/menu.txt`** — replace the item list with the trimmed tree (ids verbatim from Task 1) and keep the LX200 annotations as comments; the hardware-gating table goes in a comment block (`Focus←FOC`, `Autohome RA←HSAH`, `Autohome DEC←HSAV`, `AZ←AUTO_AZ|AUTO_AZ_ALT`, `ALT←AUTO_ALT|AUTO_AZ_ALT`); the sim always shows everything.
- Modify: `CLAUDE.md` — menu section: trimmed tree, jog mode, confirm screens with No-default, `:XGM#` gating, idle long-press Stop-all, `:XGM#` caching in Controller.
- Modify: `docs/superpowers/specs/2026-11-22-display-interaction-design.md` — no; **Test:** hardware steps below.

**Interfaces:**
- Consumes: everything from Tasks 1-4.
- Produces: the running firmware.

- [ ] **Step 1: Wire `main.cpp`**:
  - `#include "MenuDefinition.hpp"`, `"JogController.hpp"`; globals `JogController jog;` + a `RotaryAsJogInput` adapter over `rotaryInput` (maps `isUpPressed()`→`upPulse` semantics: consume-one-per-call wrappers using the encoder's edge behavior).
  - `buildMenuNavigator(const char* xgm)` helper: parse `MenuDefinition::buildMenuDef(xgm)`, wire `onMenuActivity`, return nav; called at boot with nullptr and again when `controller->getLastXGM()` first becomes non-empty (menu-closed swap is fine — the reply arrives at connect).
  - `tuh_cdc_umount_cb` clears the profile (`controller->setLastXGM("")`) so a mount swap re-queries.
  - Loop, connected && `getLastXGM()` empty && no XGM job in flight → enqueue `:XGM#` FromController once (bool flag reset by umount).
  - `onMenuActivity` rewrite: Exit → close; `111/121/311/321` → `menuNav->returnToParent()`; `ID_JOG` → close menu, `jog.setActive(true)`, enqueue `:XSM1#`; Tracking toggle → `trackingToggleToCommand(value)`; editors → `updateTargetState`/`updateSetupState`; direct ids via `menuIdToCommand` then `if (menuActionClosesMenu(id)) menuNav->setActive(false)` after enqueue; `ID_GOTO_OBJECT`/`ID_TARGET_SLEW` → `enqueueSlew` + close; `ID_SETUP_HA_STORE` → `composeShCommand` + `:MT1#` + close; date/time/UTC stores → their composes (no close).
  - Idle branch: long-press (encoder select held ≥ 750 ms, edge-tracked) → enqueue `:Q#`; reuse `waitForSelectRelease` so the release doesn't open the menu.
  - Jog branch (before idle/menu rendering): `jog.tick(...)` per loop, enqueue returned commands; render the jog screen (`JOG <axis>` title, two axis rows with signed speeds from `raSpeed()/decSpeed()`, active axis inverse-highlighted); on `!jog.isActive()` transition out → enqueue nothing extra (tick already emitted the exit sequence), set `waitForSelectRelease`, reopen the menu (`menuNav->setActive(true)`).
  - Refresher gate: `if (mountSerial->connected() && !jog.isActive() && statusRefresher.shouldRequest(millis()))`.
  - Tracking sync: when `controller->getLastGXTime()` changes and the menu is open, `rootMenu->setItemValueById(ID_TRACKING, gxActivityHasT ? "ON" : "OFF")` (activity letter T = field 1 char 2 of the cached reply).
- [ ] **Step 2: Run `pio run` — SUCCESS**; `pio test -e native` — all suites PASS (main.cpp is excluded from native; the suites prove nothing broke underneath).
- [ ] **Step 3: Update `OledMenu/sim/menu.txt` and `CLAUDE.md`** as listed above. Re-run the OledMenu sim against the new menu.txt: `pio run -e native && .pio/build/native/program.exe --tree menu.txt` in `../OledMenu/sim` — expect the trimmed tree, 11 submenus, and a clean parse.
- [ ] **Step 4: Flash and smoke-test**: `pio run -t upload`; via the COM3 PowerShell probe: `:GVN#` → `V1.13.20#`, `:GX#` → status string; serial monitor shows `Menu initialized.` (def parsed) and one `:XGM#` fetch after connect; unplug and replug the mount USB and confirm the monitor shows a second `:XGM#` fetch (profile reset on umount). Note in the commit message: on-device menu navigation awaits the wired OLED/encoder (hardware not attached yet).
- [ ] **Step 5: Commit** `git add src/main.cpp ../OledMenu/sim/menu.txt CLAUDE.md && git commit -m "Wire trimmed menu, jog mode, gating and Stop-all into the firmware"` — push the branch.

---

## Verification (whole feature)

- `pio test -e native` — every suite green (Task 1-4 suites + pre-existing).
- `pio run` + `pio run -t upload` — build and flash.
- Hardware smoke: pass-through `:GVN#`/`:GX#`/`:XGM#` via COM3; monitor log shows menu init + XGM fetch. Interactive jog/menu verification is blocked on the OLED/encoder wiring and stays open until then.
