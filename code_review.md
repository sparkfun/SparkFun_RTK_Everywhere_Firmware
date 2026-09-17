# RTK Everywhere Firmware — Code Review

## Progress

- [x] Triage: categorize issue types
- [x] Deep dive: Error handling (HIGH)
- [ ] Deep dive: Memory management — large stack buffers (HIGH)
- [ ] Deep dive: FreeRTOS task safety — mutex coverage (MEDIUM)
- [ ] Deep dive: NVM/Settings — validation and corruption risks (MEDIUM)
- [ ] Deep dive: Network robustness — timeout/close logic (MEDIUM)
- [ ] Deep dive: Code duplication — parallel GNSS implementations (MEDIUM)
- [ ] Deep dive: Long functions and maintainability (MEDIUM)
- [ ] Deep dive: Dead code and magic numbers (LOW)

---

## Triage Summary

| Severity | Category | Instances |
|----------|----------|-----------|
| HIGH | Error handling — unchecked allocs, silent failures | Dozens |
| HIGH | Memory management — large stack buffers in task contexts | Handful |
| MEDIUM | FreeRTOS task safety — ring buffers and settings w/o mutex | Handful |
| MEDIUM | NVM/Settings — no range validation, no CRC, no atomic writes | Handful |
| MEDIUM | Network robustness — socket/connection leaks on retry | Handful |
| MEDIUM | Code duplication — 4 GNSS drivers, NVM parse/update/stringify | Dozens |
| MEDIUM | Long functions — menuCommands (3454 lines), WiFi (3362), Tasks (3021) | Dozens |
| LOW | Dead code / commented blocks | Dozens |
| LOW | Magic numbers | Many |

---

## 1. Error Handling (HIGH)

Hot spots: **Begin.ino** and **Tasks.ino**

### Unchecked heap allocations ✅ DONE

- **Begin.ino ~918** — `new SdFat()` — already correctly handled (false positive)
- **Begin.ino ~1048** — `rbOffsetArray` — already correctly handled (false positive)
- **Begin.ino ~1345** — `new BQ40Z50` — already correctly handled (false positive)
- **Begin.ino ~1146** — `new HardwareSerial(1)` for `serialGNSS` — **FIXED**: added NULL check; logs error, clears `gnssUartPinnedTaskRunning`, calls `vTaskDelete`
- **Begin.ino ~1182** — `new HardwareSerial(2)` for `serial2GNSS` — **FIXED**: added NULL check; logs error and returns early

### Unchecked xTask/xSemaphore returns ✅ DONE

- **Begin.ino ~898** — `sdCardSemaphore` creation not checked; `gotSemaphore = true` set unconditionally even on failure — **FIXED**: wrapped in braces, break on nullptr
- **Tasks.ino ~1247** — `ringBufferSemaphore` in `processUart1Message` unchecked before `xSemaphoreTake` — **FIXED**: nullptr check added, return on failure; changed NULL→nullptr
- **Tasks.ino ~1619** — `ringBufferSemaphore` in `handleGnssDataTask` same issue — **FIXED**: nullptr check added, continue on failure; changed NULL→nullptr
- **Begin.ino ~1065** — `GnssUartStart` task creation unchecked, infinite-hang wait if failed — **FIXED**: check return, clear flag on failure
- **Begin.ino ~1627** — `I2CDetect` task creation unchecked, infinite-hang wait if failed — **FIXED**: check return, skip wait on failure
- **Begin.ino ~1748** — `I2CStart` task creation unchecked, infinite-hang wait if failed — **FIXED**: check return, skip wait on failure
- **Begin.ino ~1530** — `BtnCheck` task creation unchecked (silent) — **FIXED**: logs error on failure
- **Begin.ino ~1601** — `IdleTask` x2 creation unchecked (silent, debug-only) — **FIXED**: logs error on failure
- **Begin.ino ~1969** — `SDSizeCheck` task creation unchecked (silent) — **FIXED**: logs error on failure
- **Tasks.ino ~2803, ~2813, ~2823** — `gnssRead`, `handleGNSSData`, `btRead` task creations unchecked — **FIXED**: logs error on failure

### Missing recovery / infinite hang risk ✅ DONE

- **NVM.ino `loadSettingsUsingTempSetting` ~125** — on `rtkMalloc` failure, previously fell back to loading directly into live `settings` with no recovery if parse failed — **FIXED**: keeps `tempSettings = &settings` fallback (try anyway), but now calls `getDefaultSettings(&settings)` if the load itself fails
- **NVM.ino `loadSettingsPartial` ~383** — same pattern — **FIXED**: same approach

### Recommended fix

Add a `rtkTaskCreate()` helper wrapping `xTaskCreatePinnedToCore` that checks the return value and calls `reportFatalError()` on NULL — the same pattern already used in Tasks.ino for `rtkBuffer`. Covers ~9 of these sites in one sweep.

---

## 2. Memory Management — large stack buffers (HIGH)

### Fix approach

`rtkMalloc` / `rtkFree` for all large buffers — routes to `ps_malloc` (PSRAM) when available. `static` local goes to BSS (internal RAM), which is the wrong choice given very limited local RAM. Pattern: named size constant + `rtkMalloc` + nullptr check + `rtkFree` at narrowest scope.

### HIGH — frequently-called functions ✅ DONE

- **NTP.ino `ntpServerUpdate` ~805** — `char ntpDiag[768]` — **FIXED**: heap-allocated inside `NTP_STATE_SERVER_RUNNING` case only; frees on early-out and at end of case
- **LoRa.ino `updateLora` ~280 & ~330** — `uint8_t rtcmData[512]` declared twice in separate cases — **FIXED**: single `loraRtcmBufferSize` constant; `rtkMalloc` inside each `if (loraAvailable())` block, freed after use

### MEDIUM — network state machine updates ✅ DONE

- **NtripClient.ino `ntripClientUpdate` ~775** — `char response[512]` — **FIXED**: heap-allocated at else-block entry, freed before closing brace; OOM breaks to next update cycle
- **NtripServer.ino `ntripServerClientUpdate` ~1007** — `char response[512]` — **FIXED**: same pattern
- **TcpServer.ino `tcpServerClientUpdate` ~372** — `char response[512]` at function scope — **FIXED**: removed from function scope; heap-allocated inside `TCP_SERVER_CLIENT_GET_REQUEST` case with explicit braces; all `sizeof(response)` → named constant; freed before `break`
- **LoRa.ino `loraSetupCommon` ~735** — `char response[512]` — **FIXED**: heap-allocated at if-block entry, freed before closing brace; OOM returns early
- **LoRa.ino `loraGetVersion` ~1107** — `char response[512]` — **FIXED**: heap-allocated inside debug block; on OOM logs error and skips attribute query (debug-only path); freed after use

### MEDIUM — infrequent paths (pending)

- **GNSS_Mosaic.ino `getMode` ~1127** — `char receiverResponse[500]`
- **Tilt.ino `im19CheckResponse` ~1559** — `uint8_t buf[350]` (OTA update only)
- **Tilt.ino `im19PumpStreamToDevice` ~1763** — `uint8_t buffer[512]` (OTA update only)

### LOW — infrequent paths (pending)

- **menuBase.ino ~280 & ~707** — `char arpPrompt[300]`, `char apcPrompt[300]` (menu only)
- **menuFirmware.ino ~1230** — `uint8_t buffer[512]` (OTA update only)

---

## 3. FreeRTOS Task Safety (MEDIUM)

### Semaphore inventory

| Semaphore | Where created | Who takes/gives |
|-----------|--------------|-----------------|
| `sdCardSemaphore` | Begin.ino (explicit init) | Main loop, Tasks.ino, NVM.ino, Logging.ino, WebServer.ino, SD.ino |
| `ringBufferSemaphore` | Tasks.ino — lazy-init on first use | `processUart1Message` (gnssReadTask) + `handleGnssDataTask` |
| `ntripServer->serverSemaphore` | NtripServer.ino — lazy-init per-method | `updateTimerAndBytesSent` (handleGnssDataTask) + `millisSinceTimer` (main loop) |
| `webServerMutex` | WebServer.ino — explicit init with null check | WebServer handlers (HTTP callbacks) |
| `pushGPGGA::reentrant` | GNSS.ino — static local init | Wherever GNSS callback fires |

### Issue 1 — `serverSemaphore` lazy-init race (REAL BUG)

Each of the 12 methods in `NTRIP_SERVER_DATA` does:
```cpp
if (serverSemaphore == NULL)
    serverSemaphore = xSemaphoreCreateMutex();
if (xSemaphoreTake(serverSemaphore, ...))
```
`updateTimerAndBytesSent` is called from `handleGnssDataTask`; `millisSinceTimer` from the main loop. If both tasks first enter these methods simultaneously while `serverSemaphore` is still NULL, both call `xSemaphoreCreateMutex()`, and one returned handle is overwritten and leaked. Neither path checks the creation result before taking.

Recommended fix: Pre-create in `ntripServerStart` (which runs single-threaded in the main loop before CASTING state is entered), and add a NULL check after creation.

### Issue 2 — `ringBufferSemaphore` lazy-init race (POTENTIAL BUG)

`processUart1Message` (in `gnssReadTask`) and `handleGnssDataTask` both lazy-init the global `ringBufferSemaphore`. If both tasks start and reach the init check simultaneously, both create a mutex and one is lost. Previous session added a NULL check after creation but not the race itself.

Recommended fix: Create `ringBufferSemaphore` once in `beginGNSS()` or main `setup()` before either task is spawned.

### Issue 3 — `webServerMutex` unchecked takes (LOW — safe by design)

Lines 1497, 2138, and 2244 call `xSemaphoreTake(webServerMutex, portMAX_DELAY)` with no NULL guard. If mutex creation failed at startup, these would crash. In practice, the webserver init failure `break` exits `webServerAssignResources` early and the server never registers handlers, so these lines are never reached. Safe by design, but fragile.

### Issue 4 — `pushGPGGA::reentrant` static-init race (LOW — likely safe)

`static SemaphoreHandle_t reentrant = xSemaphoreCreateMutex()` uses a GCC static-local that is typically NOT thread-safe in embedded builds. First call is almost certainly from single-threaded GNSS init, so the race window is effectively zero. Document-only.

### Issue 5 — `settings` struct accessed from multiple tasks without mutex (DESIGN PATTERN)

Tasks.ino alone has 99+ direct `settings.X` reads, with no mutex protecting the struct. In practice: nearly all task-side accesses are reads; writes happen only from NVM.ino in the main loop; individual field reads are word-aligned and atomic on the Xtensa core. Acceptable design trade-off for this firmware, but any future multi-field atomic update would need care.

### Take/give balance

- `sdCardSemaphore`: all give paths gated on `gotSemaphore` or equivalent — balanced ✅
- `ringBufferSemaphore` in `processUart1Message` (lines 1257→1503): no early return/continue inside held section — balanced ✅
- `ringBufferSemaphore` in `handleGnssDataTask` (lines 1632→2066): no early return/continue inside held section — balanced ✅

---

## 4. NVM/Settings (MEDIUM)

*To be filled in after deep dive.*

---

## 5. Network Robustness (MEDIUM)

*To be filled in after deep dive.*

---

## 6. Code Duplication (MEDIUM)

*To be filled in after deep dive.*

---

## 7. Long Functions / Maintainability (MEDIUM)

*To be filled in after deep dive.*

---

## 8. Dead Code & Magic Numbers (LOW)

*To be filled in after deep dive.*
