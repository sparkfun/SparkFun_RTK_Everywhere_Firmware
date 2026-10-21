# E-Paper Display Redesign: Implementation Plan

Based on Juan Cortes's "FP-E E-Paper Display: My Proposal" (184x88 e-paper). Applies to the 184x88 e-paper only. The OLED code paths and the shared `icons.h` bitmaps stay as they are.

## Architecture (protects the OLED)

- **New file `DisplayEPaper.ino`** holds all the new 184x88 layout code: the top strip, the big number, the satellite block, the fixed-slot bottom row, and the base and power-off screens. At the top of the rover/base cases in `displayUpdate()` (`Display.ino`), check `present.display_type == DISPLAY_184x88` and hand off to it. The shared `paint*()` functions stay as they are, so the 64x48 and 128x64 OLEDs are untouched.
- **New file `icons_epaper.h`** holds the new e-paper bitmaps. `icons.h` arrays are left alone, since the OLEDs use them too.
- **Fonts:** small text uses Terminus Bold 13/15 px and the big digits use Spleen 40 px (BSD-2 license). Both are generated with the existing `Tools/EPaper_Emulator/export_bigdigits.py` pipeline, which already exports Terminus for `fontBigDigits.h`.
- Once the new path is in place, the 2x machinery (`iconPropertyList2x`, `displayBitmap2x`, `pushIcon2x`) has no remaining 184x88 users and can be deleted.
- **Juan's work is imported from his PDF.** His 46 frames were reduced from 2x to native 184x88 (`assets/juan_proposal/frames.json`). His icons are cut from those frames, so they are pixel exact. His top strip is Terminus Bold 20 (`ter-u20b`, 13 px capitals) in a 16 row strip. His big digits are Spleen 32x64 (40 px digits).
- **Mockups:** `render_proposal.py` writes `output/proposal/sheet.png`, which shows today's firmware, Juan's frame and the proposed frame side by side for every scenario.

## Phase 0: Readability gate for the white-on-black strip (do first)

The rest of the layout depends on this result.

**Status: built (scenarios S1-S5 in `DisplayTest.ino`), waiting on the camera run.** Each scenario shows the four fix words (NO FIX, GNSS, RTK FLOAT, RTK FIX) with the Bluetooth ID and battery:

- **S1:** white on black, Terminus Bold 20, 16 row strip (Juan's exact strip)
- **S2:** white on black, Terminus Bold 20, 20 row strip (more margin)
- **S3:** fallback: black text on white with a 2 px rule
- **S4:** ghosting test. One strip cycles the four words through 24 partial updates (about 48 s, with the periodic full update suppressed), then holds RTK FIX.
- **S5:** the same final frame after a full update, for comparison with S4

Each strip scenario starts with a full update. Photograph with `Tools/DisplayCamera/display_test.py --port COM6`:
- S1-S3 and S5 with the default settle time
- S4 with `--settle 60`

**Decision rule:** if S1 is crisp and S4 shows acceptable ghosting compared with S5, use Juan's strip. Otherwise use S2 or S3. The rest of the plan works either way.

**Results (2026-10-09, camera photos in `Tools/DisplayCamera/output/20261009_*`):**
- **Readability:** S1 (Juan's 16 row strip) and S2 are crisp and easy to read. White on black is not a readability problem.
- **Partial updates:** S4 fails badly. After 24 partial cycles the strip shows RTK FIX overlaid with outlines of the other words. White text on black is not driven cleanly by the partial waveform. S5 (full update) is clean.
- **Black on white:** S6 (added) runs the same 24 partial cycles on a black-on-white strip, and it stays clean.
- **Consequence:** a white-on-black strip can only change with a full update. That covers the fix word, the Bluetooth ID appearing or disappearing, and the battery icon. Either every strip change triggers an immediate full update (a 2 s black/white flash, debounced so FIX↔FLOAT flapping doesn't flash repeatedly), or the strip goes black on white with a rule (S3 / S6 style), which works with partial updates.
- **Library quirk:** `rectangleFill()` 2 px rules at rows 18-19 and 84-85 come out about 4 px tall and overlap the text above (S3, S6). Rows 40-41 and 62-63 are drawn correctly. This needs a look before any layout relies on thin filled rectangles.
- **Boot (Phase 1):** logo + STARTING is on the panel 2 s after reset and holds without redraws until the status screen.

## Phase 1: Fewer refreshes, no blinking

This phase doesn't depend on the layout and is low risk, so it can ship on its own.

**Status: implemented and compiling, not yet run on hardware.** Every change is limited to the 184x88 e-paper. The OLED's `display()` ignores the partial/full flags, and frame hashing only runs for e-paper.

| Item | Before | Now |
|---|---|---|
| Boot | Library `begin()` full refresh (white) → logo full refresh → model/version partial → wait for `minSplashFor` (1 s) and `finishDisplay()` (2 s) → "Rover Started" full refresh plus a 500 ms delay → status screen full refresh | Library `begin()` full refresh (white) → **logo + STARTING** as a partial update, held through configuration (unchanged frames aren't pushed) → status screen as a partial update. No splash waits, no "Rover/Base Started" message. That removes 3 full refreshes and about 3.5 s of fixed delays. |
| Content-type switches | Every splash/message/status switch forced a full refresh | Partial update. A full update happens only when requested (`requestFullUpdate()`), on the powered off screen, on a state change, or from the backstop. |
| Periodic full refresh | Every 60 s | One full refresh per state change (`fixStateChanged()`), **limited to one per 30 s**, plus a backstop every 10 min |
| Unchanged frames | Pushed to the panel every 2 s | Every draw call in `HYBRID_DISPLAY` is folded into an FNV-1a frame hash. A partial update of an identical frame is skipped. No library change was needed. |
| Blinking | Duty masks such as `0b01010101` | On 184x88, any icon with a non-zero duty is drawn steady |
| Power off | "Shutting Down..." message, 2 s delay, `reset(true)` (a full refresh to white), then the off screen (another full refresh) | One full refresh straight to the off screen |
| Off / boot screens | Logo + model | Shared layout (`paintPowerScreen184x88()`): logo, status word (POWERED OFF / STARTING / CHARGE BATTERY), and a detail line. Boot changes only the words. |

**Remaining boot full refresh:** the SSD168x library's `init()` always does a full update to white inside `begin()`. Removing it needs a library option, for example `begin(..., bool initialUpdate)`. With that option, boot can preload the controller's previous-image RAM with the inverse of the STARTING frame, then do one partial update. That would make boot zero full refreshes, with only the word changing visibly.

## Phase 2: Rover screen (Juan's layout, with changes)

- **Top strip:** fix word (NO FIX / GNSS / RTK FLOAT / RTK FIX / PPP / PPP FIX), the 6-character BT ID, and a small battery.
  - **The BT ID shows only while no phone is connected.** Once connected, it's removed and the Bluetooth icon in the bottom row shows the connection.
- **Big accuracy number:** round first, then pick mm, cm or m (Juan's §5 fix). Show >30 m above 30 m.
- **Satellite and count:** fixed position on the right. Show the real count at NO FIX instead of forcing it to 0.
- **NO DATA:** shown when the position stops updating.
- **Bottom row:** fixed slots (connections | corrections | device | recording). Empty slots stay empty.
- **Logging:** static page icon plus a "writing" dot. Pulse frames are no longer used on e-paper.

## Phase 3: Icon decisions

| Icon | Decision |
|---|---|
| BT, WiFi, battery, logging, arrows, castle, most dynamic models | Juan's versions |
| Sea, Airborne 4g | Juan's (they were missing; the phone showed in their place) |
| Mower, e-scooter, tractor | Juan's redraws |
| **LoRa, USB, NTRIP (SNIP), PointPerfect IP** | **The originals, redrawn at about 1.3x** (LoRa 19x19, USB 17x19, SNIP 19x13, pin 19x18). At 1x the originals looked weak next to Juan's 20 px icons. The redraws keep the original shapes: the lattice tower with radiating rays, the trident, the SNIP double diamond, and the pin over the target. The art is in `render_proposal.py`. |
| **Radio port** | New icon: a handheld radio with an antenna. Shown with the links when RTCM output on the radio port is enabled. |
| **Tilt** | New design (below) |
| **Portable model** | New design (below) |
| **No-output warning** | New design (below) |

**Tilt, 16x17.** A plumb arrow next to a tilted 2 px pole with the receiver on top, and no baseline (Juan's version read as a "greater than" sign):
```
...#.......####.
..###.....######
.#.#.#....######
...#......######
...#.......####.
...#.......##...
...#......##....
...#......##....
...#.....##.....
...#....##......
.......##.......
.......##.......
......##........
.....##.........
....##..........
....##..........
...##...........
```
Also tried: a dashed vertical line with a pole reads as a checkmark, and a pole on its own reads as a slash. The arrow version was the clearest of the four variants rendered.

**Portable, 15x13.** A carry case with a handle. "Portable" without suggesting a phone, which could be mistaken for a Bluetooth connection:
```
.....#####.....
.....#...#.....
###############
#.............#
#.............#
#.............#
#.....###.....#
#######.#######
#.....###.....#
#.............#
#.............#
#.............#
###############
```

**No-output warning, about 15x13:** a filled triangle with a white "!". Used on the base screen (Phase 4).

## Phase 4: Base screens

**Recommendation: show link state instead of a packet count.** The count only proves the GNSS is producing RTCM. `rtcmPacketsSent++` in `processRTCM()` (`Base.ino`) counts every packet even when no transport is active, so today the number climbs while nothing is going anywhere. The base screen has three states:

The top strip shows the base mode (BASE, TEMP BASE, BASECAST, SURVEY-IN). These are short enough to fit beside the Bluetooth ID; "BASE FIXED" is not. The body shows link health:

| State | Condition | Screen |
|---|---|---|
| **SENDING** | At least one link active and `rtcmLastPacketSent` < 5 s ago | Check mark, "SENDING", and a small count underneath ("999 sent", "12k sent"). The bottom row shows **only the links actually carrying data**. |
| **STALLED** | At least one link active, but no RTCM for over 10 s | Warning triangle, "NO RTCM" |
| **NO OUTPUT** | Base running, no transport enabled or connected | The strip reads NO OUTPUT. Warning triangle, "NO OUTPUT / Add a link". Forces one full refresh so it's crisp. |

When healthy this screen only changes with the small count and the heartbeat line.

- **Heartbeat line (kept):** a 1 px line under the strip that grows through each minute, on rover and base screens. It steps every 10 s rather than every 2 s, to keep partial updates down, and starts over each minute.
- **Link test:** `baseBroadcastIsActive()` (`Base.ino`) covers ESP-NOW, LoRa, NTRIP server and caster. TCP/UDP server clients, BT and the radio port need to be added.
- **Radio port (decided):** don't force the radio port on. If RTCM output on the radio port is enabled, it counts as a link and the radio port icon is shown. If it is disabled and no other link is active, NO OUTPUT is shown.
- **Survey-in:** keep Juan's time bar (it only grows). Add the accuracy goal on its own line (`MEAN 1.23m` / `GOAL 2.00m`), with the time beside a shorter bar. Survey-in needs both time and accuracy, so the bar alone can show 100% while accuracy still isn't met.
- **BASECAST:** the IP address stays on screen, as Juan proposed.
- **Survey-in failed:** add the message screen.

## Phase 5: Power-off and battery

- **Off screens:** POWERED OFF is done in Phase 1 (`paintPowerScreen184x88()`). Still to do: CHARGE BATTERY and AUTO OFF use the same layout. AUTO OFF hooks into the existing `shutdownNoChargeTimeoutMinutes` path (`System.ino`).
- **Never block boot (decided).** The boot check (`beginFuelGauge()`, `Begin.ino`) stays at 5%, but it becomes a warning only. It no longer shuts the unit down. The MAX17048 path also needs fixing: it decides "not charging" from `lipo.getChangeRate()` read right after power-on, when the gauge has had no time to measure a rate. So a unit plugged in to charge at 4% can still shut itself down. Use `isCharging()` (the charger's state), as the BQ40Z50 path already does.
- **Runtime cutoff (decided):**
  - Condition: `batteryLevelPercent <= 1` and not charging, debounced over about 30 s of readings. Kept at 1% so no usable battery is taken away from the user.
  - Action: paint CHARGE BATTERY, then call `powerDown()`.
  - Today there's no runtime check, so the screen freezes on the last fix.
- **Shutdown:** done in Phase 1 (straight to the off screen, one full refresh).

## Phase 6: New screens (optional, later)

- **Info screen:** firmware, IDs, IP, GNSS, battery. This is where the rover's IP address moves to.
- **Help QR:** needs a real icon-guide URL.
- **Setup menu:** add the check mark for the current mode.

These are new UI, so they can follow the core redesign.

## Phase 7: Verification

- Update the DisplayTest scenarios to match Juan's list, plus:
  - Juan's edge cases (NO DATA, antenna open, 1k/12k/1M counts, 0.995 m, >30 m)
  - the three base states
- Re-run the camera loop.
- Update `docs/displays.md` with the new icon guide.

## Decisions

1. **Boot battery check:** stays at 5%, warning only. Boot is never blocked. Power down only at 1% (Phase 5).
2. **Radio port:** not forced on. If enabled, it counts as a link and shows the radio port icon. If disabled and nothing else is active, show NO OUTPUT.
3. **Heartbeat line:** kept.

## Open questions

1. **SSD168x library option** to skip the full update in `init()`. This is needed to make boot zero full refreshes (see Phase 1).
