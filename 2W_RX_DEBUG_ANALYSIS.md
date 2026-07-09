# 2W RX Communication Debugging Analysis

## Executive Summary
**Issue**: 2W (bidirectional) RX callbacks are not being received after successful 1W pairing.
- ✅ 1W send works correctly
- ❌ 2W receive is not triggered
- 🎯 **Root Cause**: Missing 2W listen mode activation + Possible TX→RX transition issue

---

## Detailed Root Cause Analysis

### 🔴 CRITICAL ISSUE #1: No 2W Scan Activation After Pairing

**Location**: `src/main.cpp`, function `msgRcvd()`, case `0x38` (RECEIVED_LAUNCH_KEY_TRANSFERT)

**Current Code** (~line 438-500):
```cpp
case iohcDevice::RECEIVED_LAUNCH_KEY_TRANSFERT_0x38: {
    // ... pairing handshake ...
    radioInstance->send(packets2send);
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
    break;  // ❌ ENDS HERE - NO 2W LISTENING STARTED
}
```

**Missing Code**:
After successfully sending `SEND_KEY_TRANSFERT_0x32` (0x32), the radio should enter 2W listening mode:
```cpp
// Should be added after send():
radioInstance->startTwoWScan(
    TWOW_SCAN_WINDOW_MS,    // 8000 ms listen window
    TWOW_SCAN_INTERVAL_US   // 2700 us dwell per channel
);
```

**Why This Matters**:
- `startTwoWScan()` configures the radio to hop through all configured frequencies with fast dwell times (2.7ms per channel)
- Without this, the radio remains on the 1W channel (868.95 MHz) forever
- 2W responses from the device will arrive on FHSS (frequency hopping) channels
- Device will send initial 2W packet (0x29) and subsequent challenge requests (0x3C) that will be missed

**Evidence**:
- `startTwoWScan()` exists and is implemented (`src/iohcRadio.cpp` line 221)
- It's called at radio init for 1W mode but **never called for 2W**
- 2W constants are defined but unused in pairing flow

---

### 🔴 HIGH PRIORITY ISSUE #2: TX→RX Transition Not Restored

**Location**: `src/iohcRadio.cpp`, function `packetSender()`, ~line 511

**Current Code**:
```cpp
radio->txComplete = false;
radio->setRadioState(RadioState::TX);
Radio::setStandby();
Radio::clearFlags();
Radio::writeBytes(REG_FIFO, radio->iohc->payload.buffer, radio->iohc->buffer_length);
Radio::setTx();
//Radio::setRx();  // ❌ COMMENTED OUT - This is the problem!
```

**Expected Code**:
```cpp
//Radio::setRx();  // ✓ Should NOT be commented out
```

**Why This Matters**:
- SX1276 must be in RX mode to receive packets
- After TX completes, radio needs to return to RX
- Currently, only `lightTxTask()` calls `Radio::setRx()` at the END of the entire batch
- Between repeated packets or between commands, RX is inactive
- Any arriving 2W responses during TX batch are missed

**Workaround Path** (less reliable):
- `lightTxTask()` ~line 284 does call `Radio::setRx()` after loop completes
- But this only works if ALL packets finish first
- Doesn't help if device sends response immediately after first TX

**Code Trail**:
```cpp
// lightTxTask() - src/iohcRadio.cpp line 280+
while (radio->txCounter < radio->packets2send.size()) {
    Radio::setStandby();
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, ...);
    Radio::setTx();
    // ... repeat handling ...
}
// Only after ALL packets sent:
Radio::setRx();  // ✓ Finally switches back!
radio->setRadioState(iohcRadio::RadioState::RX);
```

---

### 🟡 MEDIUM PRIORITY: Interrupt Handler State During 2W Scan

**Location**: `src/iohcRadio.cpp`, interrupt handlers line 45-80

**Status**: Interrupt handlers are correct but depend on radio being in RX:

```cpp
void IRAM_ATTR handle_payload_interrupt_fromisr() {
    if (!digitalRead(RADIO_PACKET_AVAIL)) {
        return;  // ✓ Safely ignores if no packet
    }

    if (iohcRadio::radioState == iohcRadio::RadioState::TX) {
        iohcRadio::txComplete = true;  // ✓ Handles TX done
    } else {
        iohcRadio::setRadioState(iohcRadio::RadioState::PAYLOAD);
        vTaskNotifyGiveFromISR(handle_interrupt, &xHigherPriorityTaskWoken);
    }
}
```

**Issue**: This works fine IF:
- ✓ Radio is in RX mode (requires fix #1)
- ✓ Radio is on correct channel (requires fix #1)
- ✓ RX is entered between packets (requires fix #2)

---

## Configuration Review

### Frequencies Defined ✓
- Primary 1W: 868.95 MHz (CHANNEL2)
- 2W FHSS: Multiple channels in `frequencies[]` array
- Dwell times configured:
  - 1W: 13,520 µs (13.5 ms) per channel
  - 2W: 2,700 µs (2.7 ms) per channel - much faster for responsiveness

### Radio Mode Switching ✓
```cpp
// All these functions exist and work correctly:
Radio::setRx();      // Set to RX mode, sync size 3, clear flags
Radio::setTx();      // Set to TX mode, different sync size
Radio::setStandby(); // Intermediate state before mode change
```

### Pairing Handshake ✓
The 1W pairing sequence is implemented correctly:
```
0x28 (DISCOVER)
 → send 0x29 (DISCOVER_ANSWER) ✓
0x2C (DISCOVER_ACTUATOR)
 → send 0x2D (DISCOVER_ACTUATOR_ACK) ✓
0x38 (LAUNCH_KEY_TRANSFERT)
 → send 0x32 (KEY_TRANSFERT) ✓
 ❌ BUT: No 2W listening started here!
```

---

## Expected 2W Communication Flow

### After Successful Pairing:
```
Gateway (this device) waits in 2W scan mode
    ↓ (gateway on channel N, listening)
Device sends DISCOVER_ANSWER (0x29) on channel N
    ↓ (should trigger RX interrupt)
msgRcvd() receives 0x29
    ↓ (device indicates it's ready for 2W)
Gateway sends response on channel N+1
    ↓ (frequency hop)
Device listens on N+1, receives response
    ↓ (bidirectional link established)
Device sends/receives control frames (0x20, 0x21, etc.)
```

### Current Behavior (Broken):
```
Gateway stays on 1W channel (868.95 MHz)
Device sends 0x29 on 2W channel N
    ↓ (missed - wrong channel!)
Gateway timeout (waiting on 868.95)
Device retry timeout (no acknowledgment)
Connection fails ❌
```

---

## Recommended Fixes

### FIX #1: Enable 2W Listening (CRITICAL)

**File**: `src/main.cpp`
**Function**: `msgRcvd()`
**Case**: `iohcDevice::RECEIVED_LAUNCH_KEY_TRANSFERT_0x38`

**Before** (around line 495):
```cpp
radioInstance->send(packets2send);
digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
break;
```

**After**:
```cpp
radioInstance->send(packets2send);
digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

// 🔧 ADD THIS: Enable 2W listening after pairing
printf("2W Key Transfert sent, starting 2W scan...\n");
radioInstance->startTwoWScan(
    TWOW_SCAN_WINDOW_MS,    // Listen for 8 seconds
    TWOW_SCAN_INTERVAL_US   // 2.7ms dwell per channel
);

break;
```

**Verification**:
- Check logs for: `"2W scan started channels=..."`
- Monitor radio frequency changes with spectrum analyzer
- RX interrupt should fire when device responds

---

### FIX #2: Restore TX→RX Transition (HIGH)

**File**: `src/iohcRadio.cpp`
**Function**: `packetSender()`
**Line**: ~511

**Before**:
```cpp
Radio::setRx();
radio->setRadioState(RadioState::TX); // Stay TX until done
Radio::setStandby();
// ... TX code ...
Radio::setTx();
//Radio::setRx();  // ❌ COMMENTED OUT
```

**After** (uncomment):
```cpp
Radio::setRx();
radio->setRadioState(RadioState::TX); // Stay TX until done
Radio::setStandby();
// ... TX code ...
Radio::setTx();
Radio::setRx();  // ✓ UNCOMMENTED - Restore after each packet
```

**Alternative Approach** (if timing issues arise):
If uncommenting causes race conditions, use the working `lightTxTask()` pattern:
```cpp
// After TX mode set:
Radio::setTx();

// Small delay to let TX complete (or wait for ISR)
vTaskDelay(pdMS_TO_TICKS(50));

// Return to RX
Radio::setRx();
radio->setRadioState(iohcRadio::RadioState::RX);
```

---

### FIX #3: Monitor 2W Window Timeout (MEDIUM)

**File**: `src/iohcRadio.cpp`
**Check**: Function `tickerCounter()` / timer logic

**Verify**:
- `startTwoWScan()` sets `twoWScanUntilMs = millis() + windowMs`
- Timer expiry calls `stopTwoWScan()`
- `stopTwoWScan()` returns to 1W mode cleanly
- No resource leaks after window closes

**Current Implementation** (~line 290):
```cpp
if (radio->twoWScanActive && millis() >= radio->twoWScanUntilMs) {
    radio->stopTwoWScan();
    // Should return to normal 1W scanning
}
```

Status: ✓ Logic exists, just verify it's being called.

---

## Testing Strategy

### Step 1: Verify Pairing Still Works
```bash
1. Enable 1W pairing mode
2. Watch for: "2W Pairing Asked" message
3. Watch for: "Key Transfert" sequence
4. Confirm: All 1W pairing packets are logged
```

### Step 2: Check 2W Listening Start
```bash
1. After pairing completes, look for:
   "2W scan started channels=X dwell_us=2700"
2. If missing: FIX #1 not working
3. If present: Verify frequency changes with spectrum analyzer
```

### Step 3: Monitor RX Interrupts During 2W Scan
```bash
1. Enable interrupt logging:
   - Add printf() to handle_payload_interrupt_fromisr()
   - Add printf() to handle_sync_interrupt_fromisr()
2. During 2W window, should see:
   - PREAMBLE interrupt (preamble detected)
   - PAYLOAD interrupt (packet ready)
3. If no interrupts: Radio not in RX or wrong channel
```

### Step 4: Verify Callbacks Received
```bash
1. Monitor msgRcvd() callback:
   - Should receive 0x29 (DISCOVER_ANSWER) from device
   - Should receive 0x3C (CHALLENGE_REQUEST) from device
2. Check callback code at src/main.cpp line 193
3. Verify JSON logging output for 2W packets
```

### Step 5: Check TX Between RX
```bash
1. If FIX #2 is applied:
   - Send packet from gateway
   - Immediately check if RX can occur
   - Should not miss responses due to TX blocking
2. Monitor radio mode transitions
```

---

## Code References

### 1W Pairing Sequence (working)
- File: `src/main.cpp` lines 335-500 (msgRcvd function)
- Cases: 0x28, 0x29, 0x2C, 0x2D, 0x38

### 2W Listen Mode
- File: `src/iohcRadio.cpp` lines 221-250 (startTwoWScan function)
- Constants: `TWOW_SCAN_INTERVAL_US`, `TWOW_SCAN_WINDOW_MS`

### TX→RX Handling
- File: `src/iohcRadio.cpp` lines 280-320 (lightTxTask, packetSender)
- Radio control: `src/SX1276Helpers.cpp` lines 300-310 (setRx, setTx)

### Interrupt Handlers
- File: `src/iohcRadio.cpp` lines 45-80
- GPIO pins: `RADIO_DIO0_PIN` (payload ready), `RADIO_DIO2_PIN` (sync/preamble)

---

## Diagnostic Commands

Add these debug logs to help identify which fix is needed:

### In `msgRcvd()` after sending KEY_TRANSFERT:
```cpp
printf("DEBUG: About to start 2W scan\n");
printf("DEBUG: twoWScanActive before=%d\n", radioInstance->twoWScanActive);
radioInstance->startTwoWScan(TWOW_SCAN_WINDOW_MS, TWOW_SCAN_INTERVAL_US);
printf("DEBUG: twoWScanActive after=%d\n", radioInstance->twoWScanActive);
printf("DEBUG: Radio frequency=%" PRIu32 "\n", scan_freqs[currentFreqIdx]);
```

### In interrupt handler:
```cpp
void IRAM_ATTR handle_payload_interrupt_fromisr() {
    ets_printf("INT: PAYLOAD radio_state=%d\n", (int)iohcRadio::radioState);
    // ... rest of function
}
```

### In radio state setter:
```cpp
void iohcRadio::setRadioState(RadioState newState) {
    ets_printf("STATE: %s → %s\n",
        radioStateToString(radioState),
        radioStateToString(newState));
    radioState = newState;
}
```

---

## Risk Assessment

### Risk Level: LOW
- Changes are isolated to specific code paths
- No changes to core radio hardware interaction
- Fixes align with existing, working code patterns (lightTxTask uses these patterns)
- Fallback: Can disable 2W and stick with 1W only

### Compatibility
- ✓ No breaking changes to API
- ✓ Backwards compatible with existing 1W-only systems
- ✓ No configuration file changes needed

### Testing Impact
- ✓ Existing 1W tests should still pass
- ✓ New 2W tests can be added independently
- ✓ Can test incrementally (pairing, then listening, then RX)

---

## Summary Checklist

- [ ] FIX #1: Add `startTwoWScan()` call after successful pairing (KEY_TRANSFERT sent)
- [ ] FIX #2: Uncomment or verify `Radio::setRx()` in packetSender() function
- [ ] TEST #1: Verify 1W pairing still works without 2W changes
- [ ] TEST #2: Confirm "2W scan started" message appears after pairing
- [ ] TEST #3: Monitor radio frequency hopping during 2W window
- [ ] TEST #4: Capture 2W RX interrupts and callbacks
- [ ] BONUS: Add 2W test cases to validation suite

---

**Analysis Date**: 2026-06-09
**Based On**: Static code analysis of iohomecontrol-master
**Issue**: 2W RX callbacks not received after 1W pairing
