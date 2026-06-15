# Quick-Reference Fixes for 2W RX Issue

## FIX #1: Enable 2W Listening After Pairing (CRITICAL)

**File**: `src/main.cpp`
**Function**: `msgRcvd()`
**Find this section** (around line 495):

```cpp
radioInstance->send(packets2send);
digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);
break;
```

**Replace with**:

```cpp
radioInstance->send(packets2send);
digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

// 🔧 START LISTENING FOR 2W RESPONSE
printf("2W Key Transfert sent, starting 2W scan...\n");
radioInstance->startTwoWScan(
    TWOW_SCAN_WINDOW_MS,    // Listen for 8 seconds
    TWOW_SCAN_INTERVAL_US   // 2.7ms dwell per channel
);

break;
```

**Context** (full case block):
```cpp
case iohcDevice::RECEIVED_LAUNCH_KEY_TRANSFERT_0x38: {
    // ... key transfer code ...

    radioInstance->send(packets2send);
    digitalWrite(RX_LED, digitalRead(RX_LED) ^ 1);

    // 🔧 ADD THESE LINES:
    printf("2W Key Transfert sent, starting 2W scan...\n");
    radioInstance->startTwoWScan(
        TWOW_SCAN_WINDOW_MS,
        TWOW_SCAN_INTERVAL_US
    );

    break;
}
```

---

## FIX #2: Restore TX→RX Transition (HIGH PRIORITY)

**File**: `src/iohcRadio.cpp`
**Function**: `packetSender()`
**Find this line** (around line 511):

```cpp
    Radio::setTx();
    //Radio::setRx();
```

**Replace with**:

```cpp
    Radio::setTx();
    Radio::setRx();  // ✓ UNCOMMENTED
```

**Full context** (the packetSender function):
```cpp
// Around line 500-520 in src/iohcRadio.cpp
void packetSender(iohcRadio *radio) {
    // ... setup code ...

    radio->txComplete = false;
    radio->setRadioState(RadioState::TX);
    Radio::setStandby();
    Radio::clearFlags();
    Radio::writeBytes(REG_FIFO, radio->iohc->payload.buffer, radio->iohc->buffer_length);
    Radio::setTx();
    Radio::setRx();  // ✓ CHANGE THIS LINE: Uncomment it

    ets_printf("TX: Sent packet %d/%d at %llu us\n",
               radio->txCounter + 1,
               radio->packets2send.size(),
               esp_timer_get_time());
}
```

---

## How to Apply These Fixes

### Option A: Manual Edit
1. Open `src/main.cpp` in VS Code
2. Find line ~495 (search for "RECEIVED_LAUNCH_KEY_TRANSFERT_0x38")
3. Add the `startTwoWScan()` call after `radioInstance->send()`
4. Open `src/iohcRadio.cpp`
5. Find line ~511 (search for "//Radio::setRx();")
6. Remove the `//` to uncomment it

### Option B: Using Find & Replace
**For FIX #1** - In `src/main.cpp`:
- Find: `radioInstance->send(packets2send);\s+digitalWrite\(RX_LED, digitalRead\(RX_LED\) \^ 1\);\s+break;`
- Replace: (manually - this is complex)

**For FIX #2** - In `src/iohcRadio.cpp`:
- Find: `Radio::setTx();\s+//Radio::setRx();`
- Replace: `Radio::setTx();\n    Radio::setRx();`

---

## Verification After Applying Fixes

### Test 1: Compilation
```bash
platformio run -e esp32 -v
# Should compile without errors
```

### Test 2: 1W Pairing (Should Still Work)
```bash
1. Power on device in pairing mode
2. Watch serial output for:
   - "2W Pairing Asked"
   - "2W Device want to be paired"
   - "Key Transfert Asked"
   - "2W Key Transfert sent, starting 2W scan..." ← NEW MESSAGE
3. Pairing should complete
```

### Test 3: 2W Listen Window Starts
```bash
After pairing, serial output should show:
- "2W scan started channels=X dwell_us=2700 window_ms=8000"
- Repeated RX activity for ~8 seconds
- Then returns to "Radio RX normal channels=1"
```

### Test 4: 2W Response Received
```bash
During 2W window, should see in web UI or serial:
- "2W RX cmd=0x29 from=..." (device initial 2W response)
- "2W RX cmd=0x3C from=..." (challenge request)
- "2W RX cmd=0x21 from=..." (other responses)
```

---

## Rollback Instructions

If issues occur after applying fixes:

### Rollback FIX #1:
```cpp
// Remove these lines from src/main.cpp:
radioInstance->startTwoWScan(
    TWOW_SCAN_WINDOW_MS,
    TWOW_SCAN_INTERVAL_US
);
```

### Rollback FIX #2:
```cpp
// Re-comment this line in src/iohcRadio.cpp:
//Radio::setRx();  // Comment it back
```

---

## Expected Behavior After Fixes

### Before Fix
```
1W Pairing: ✓ Works
2W Response: ✗ Never received
Logs: No "2W scan started" message
```

### After Fix
```
1W Pairing: ✓ Still works
2W Response: ✓ Received during 8-second window
Logs: "2W scan started..." appears after pairing
      RX interrupts fire on 2W channels
      msgRcvd() callbacks for 2W packets
```

---

## Line Numbers Reference

### src/main.cpp
- Line ~335: Case `0x28` (DISCOVER)
- Line ~371: Case `0x29` (DISCOVER_ANSWER)
- Line ~395: Case `0x2C` (DISCOVER_ACTUATOR)
- Line ~415: Case `0x2D` (DISCOVER_ACTUATOR_ACK)
- Line ~438: Case `0x38` (LAUNCH_KEY_TRANSFERT) ← FIX #1 HERE
- Line ~495: Send + break (add startTwoWScan)

### src/iohcRadio.cpp
- Line ~280: `lightTxTask()` function
- Line ~284: `Radio::setRx()` call in lightTxTask
- Line ~500: `packetSender()` function
- Line ~511: `//Radio::setRx();` ← FIX #2 HERE (uncomment)
- Line ~221: `startTwoWScan()` function (reference)

---

## Additional Debug Logging (Optional)

Add these to confirm fixes are working:

### In src/iohcRadio.cpp, startTwoWScan():
```cpp
void iohcRadio::startTwoWScan(uint32_t windowMs, uint32_t dwellUs) {
    printf("DEBUG: startTwoWScan called with window=%ums dwell=%uus\n", windowMs, dwellUs);
    printf("DEBUG: Current radioState=%d\n", (int)radioState);
    // ... rest of function
}
```

### In src/iohcRadio.cpp, handle_payload_interrupt_fromisr():
```cpp
void IRAM_ATTR handle_payload_interrupt_fromisr() {
    ets_printf("INT: PAYLOAD detected, state=%d\n", (int)iohcRadio::radioState);
    // ... rest of function
}
```

---

## Estimated Impact

- **LOC Changed**: ~5 lines
- **Files Modified**: 2
- **Risk Level**: LOW (working code patterns from existing lightTxTask)
- **Compilation Time**: Unchanged
- **Runtime Overhead**: Minimal (existing feature)
- **Backward Compatibility**: ✓ Full

---

Last Updated: 2026-06-09
