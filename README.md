# Interrupt-Driven Industrial Safety Lock

## Project Overview

This project implements an interrupt-driven industrial safety lock using an Arduino-based embedded system. The system demonstrates the use of multiple hardware interrupt mechanisms, timer-based background processing, sensor event handling, and a structured **Sense-Think-Act** software architecture.

The safety lock requires **three sensor events** to occur before the main lock output is activated. Sensors 1 and 2 use a shared **Pin Change Interrupt** system, while Sensor 3 uses a dedicated external interrupt through `attachInterrupt()`.

An independent **Timer1 hardware interrupt** runs every two seconds and controls a separate heartbeat LED to demonstrate that the background timing task continues to operate independently of the main safety-lock output.

The project was developed and simulated using **Tinkercad Circuits**.

---

## System Features

* Three digital sensors used as a three-factor safety authorization system.
* Sensor 1 on **Digital Pin 8** using Pin Change Interrupt.
* Sensor 2 on **Digital Pin 9** using Pin Change Interrupt.
* Sensor 3 on **Digital Pin 2** using `attachInterrupt()`.
* Timer1 configured in **CTC mode** with a 1024 prescaler.
* Timer1 generates a hardware interrupt every **2 seconds**.
* Separate heartbeat LED for the Timer1 periodic task.
* Separate main LED for the safety-lock actuator.
* Software debounce handling for the two Pin Change Interrupt inputs.
* Independent debounce timing for Sensor 1 and Sensor 2.
* Short interrupt service routines that only capture events and set flags.
* Non-blocking Sense-Think-Act program structure.
* Serial Monitor messages provide system-state feedback.
* Main lockdown output remains active after the three sensor events have been processed.

---

## Hardware / Pin Configuration

| Component           | Arduino Pin | Interrupt / Function       | Purpose                      |
| ------------------- | ----------: | -------------------------- | ---------------------------- |
| Sensor 1            |          D8 | PCINT0                     | First safety input           |
| Sensor 2            |          D9 | PCINT1                     | Second safety input          |
| Sensor 3            |          D2 | INT0 / `attachInterrupt()` | Third safety input           |
| Main Lock LED       |         D13 | Digital Output             | Indicates lockdown is active |
| Timer Heartbeat LED |         D12 | Digital Output             | Indicates Timer1 activity    |

The sensors use the Arduino's internal pull-up resistors.

Therefore:

* **HIGH** = normal/inactive sensor state
* **LOW** = sensor activated

---

## System Architecture

The firmware follows a **Sense-Think-Act** execution pipeline:

```text
                 HARDWARE EVENTS
                       │
        ┌──────────────┼──────────────┐
        │              │              │
        ▼              ▼              ▼
      Sensor 1       Sensor 2       Sensor 3
        │              │              │
       D8             D9             D2
        │              │              │
        ▼              ▼              ▼
      PCINT          PCINT       attachInterrupt()
        │              │              │
        └───────┬──────┘              │
                ▼                     ▼
           PCINT0_vect            sensor3ISR()
                │                     │
                ▼                     ▼
             Flags                  Flag
                │                     │
                └──────────┬──────────┘
                           ▼
                    ┌─────────────┐
                    │    SENSE    │
                    │ Read events │
                    │   Debounce  │
                    └──────┬──────┘
                           ▼
                    ┌─────────────┐
                    │    THINK    │
                    │ S1 AND S2   │
                    │ AND S3?     │
                    └──────┬──────┘
                           ▼
                    ┌─────────────┐
                    │     ACT     │
                    │ Main Lock   │
                    │   LED ON    │
                    └─────────────┘


                 INDEPENDENT TIMER
                       │
                     Timer1
                       │
                 2-second CTC
                       │
                       ▼
              TIMER1_COMPA_vect
                       │
                       ▼
             timerTickOccurred
                       │
                       ▼
                     SENSE
                       │
                       ▼
              Timer Heartbeat LED
```

---

## Interrupt Configuration

### Pin Change Interrupts

Digital Pins 8 and 9 are located on Port B and belong to the same Pin Change Interrupt group.

The code configures the following:

* `PCICR` to enable the Port B Pin Change Interrupt group.
* `PCMSK0` to enable `PCINT0` and `PCINT1`.
* `PINB` to directly read the Port B input states.

Both sensors therefore use the shared interrupt vector:

```cpp
ISR(PCINT0_vect)
```

Because the interrupt vector is shared, the ISR compares the current Port B state with the previous state using XOR:

```cpp
uint8_t changedBits = currentPortB ^ portBHistory;
```

This identifies which input pin changed.

The ISR then records the change using a flag rather than performing the full sensor-processing operation.

---

## External Interrupt

Sensor 3 is connected to Digital Pin 2 and uses the Arduino's external interrupt system:

```cpp
attachInterrupt(digitalPinToInterrupt(sensorPin3), sensor3ISR, FALLING);
```

The sensor uses a falling-edge trigger because the inputs use internal pull-up resistors.

When Sensor 3 changes from HIGH to LOW:

```text
D2 HIGH
   ↓
D2 LOW
   ↓
External Interrupt
   ↓
sensor3ISR()
   ↓
sensor3Triggered = true
```

The ISR remains short and only records the event.

---

## Timer1 Configuration

Timer1 is configured manually using AVR hardware registers.

The timer operates in **Clear Timer on Compare Match (CTC) mode**.

Important configuration values are shown here:

```cpp
OCR1A = 31249;
```

```text
1024 prescaler
```

With the Arduino's 16 MHz clock,

```text
16,000,000 / 1024 = 15,625 timer ticks per second

15,625 × 2 seconds = 31,250 ticks

31,250 - 1 = 31,249
```

Therefore, the Timer1 Compare Match event occurs every approximately **2 seconds**.

The interrupt vector is as follows:

```cpp
ISR(TIMER1_COMPA_vect)
```

The ISR only sets the flag shown below:

```cpp
timerTickOccurred = true;
```

The main loop then processes the flag and toggles the separate heartbeat LED.

---

## Debounce Handling

Mechanical switches can produce several rapid electrical transitions when pressed. This is known as switch bounce and can result in multiple unwanted events.

The Pin Change Interrupt routine therefore does not perform debounce processing.

Instead, it records which pin changed:

```text
PCI ISR
   ↓
Record changed pin
   ↓
Return quickly
   ↓
Main loop
   ↓
Check debounce timing
   ↓
Confirm sensor state
```

Sensor 1 and Sensor 2 use separate debounce timestamps:

```cpp
sensor1LastTime
sensor2LastTime
```

This prevents activity on one sensor from suppressing a valid event from the other sensor.

The debounce window:

```cpp
DEBOUNCE_DELAY = 50 ms
```

---

## Sense-Think-Act Logic

### Sense

The Sense stage processes the interrupt flags and determines which events have occurred.

It handles several processes:

* Sensor 1
* Sensor 2
* Sensor 3
* Timer1 periodic events
* Debounce processing
* Serial Monitor event reporting

### Think

The Think stage evaluates whether all three safety inputs have been satisfied:

```text
Sensor 1 = TRUE
AND
Sensor 2 = TRUE
AND
Sensor 3 = TRUE
```

If all three conditions are satisfied, the system recognizes that the three-factor authorization condition has been met.

### Act

The Act stage activates the main safety-lock output:

```cpp
digitalWrite(ledPin, HIGH);
```

The main LED on D13 then remains solid ON.

The sensor event flags are subsequently cleared, but the main lock output is not switched LOW. This represents the lockdown state remaining active after the authorization events have been processed.

---

## LED Behaviour

Two LEDs are intentionally used for different purposes.

### D13 — Main Lock LED

The D13 LED represents the main safety-lock actuator.

Before the three-sensor condition is satisfied:

```text
OFF
```

After all three sensors are successfully triggered:

```text
SOLID ON
```

The LED remains ON to represent the active lockdown state.

### D12 — Timer1 Heartbeat LED

The D12 LED is controlled by the Timer1 periodic task.

It toggles every two seconds:

```text
ON
OFF
ON
OFF
...
```

This provides a visual indication that the Timer1 background task is continuing to operate.

Keeping the LEDs separate prevents the periodic timer task from interfering with the main safety-lock output.

---

## Serial Monitor Output

The system provides Serial Monitor messages to make the different stages of operation visible.

Typical output:

```text
[SENSE-EVENT] PCINT Vector 0 -> Pin 8 (Sensor 1) Pressed.
[SENSE-EVENT] PCINT Vector 0 -> Pin 9 (Sensor 2) Pressed.
[SENSE-EVENT] External Interrupt -> Pin 2 (Sensor 3) Pressed.

[SENSE-TIME] Timer1 Tick! 2-Second Interval Reached.
[ACT-TIME] Periodic Heartbeat LED Toggled.

[THINK] Security Verification: All three sensor flags verified active.

[ACT-EVENT] System Main Lock Engaged -> LED SOLID ON

[SYSTEM] Operations complete. Sensor events cleared. LOCKDOWN REMAINS ACTIVE.
```

The Serial Monitor output can therefore be used alongside the LEDs to verify the system's behaviour during simulation.

---

## Interrupt Design

The interrupt service routines are deliberately kept short.

The ISRs do not perform complex processing:

* Serial communication
* Long calculations
* Debounce handling
* Multi-stage decision logic
* Extended output processing

Instead, they record the event using flags.

This allows the main loop to handle the processing through the Sense-Think-Act structure.

The main interrupt-related flags are declared using `volatile` because they can be modified asynchronously by interrupt service routines.

---


## Simulation & Testing Procedure

The system was developed and tested using **Tinkercad Circuits**.

To reproduce the main system behaviour:

1. Start the Tinkercad simulation.
2. Open the Serial Monitor.
3. Confirm that the system initialization message appears.
4. Activate Sensor 1 on D8.
5. Confirm the Sensor 1 event is reported.
6. Activate Sensor 2 on D9.
7. Confirm the Sensor 2 event is reported.
8. Activate Sensor 3 on D2.
9. Confirm the Sensor 3 external interrupt event is reported.
10. Once all three sensors have been triggered, observe the Think-stage verification message.
11. Confirm that the D13 main lock LED turns solid ON.
12. Confirm that the Serial Monitor reports that the lockdown state is maintained.
13. Confirm that the D12 heartbeat LED continues toggling every two seconds. 

Screenshots from the Tinkercad simulation are included in this repository as supporting material.

---

## Repository Contents

The repository structure:

```text
Industrial-Safety-Lock/
│
├── README.md
│
├── src/
│   └── IndustrialSafetyLock.ino
│
└── screenshots/
    ├── Circuit&SerialMonitor.png
    └── CircuitSchematic.png
```
---

## Key Learning Outcomes

This project demonstrates practical understanding of the concepts listed below:

* Pin Change Interrupts
* External interrupts using `attachInterrupt()`
* AVR interrupt vectors
* Timer1 hardware configuration
* CTC timer mode
* Timer prescalers
* Compare Match interrupts
* Direct register manipulation
* `volatile` shared state
* Interrupt service routine design
* Software debounce
* Event flags
* Non-blocking embedded programming
* Sense-Think-Act architecture
* Separation of timer and actuator outputs
* Multi-sensor event coordination

---

## Conclusion

The completed system demonstrates how an embedded controller can respond to multiple asynchronous hardware events while simultaneously performing an independent periodic background task.

Sensors 1 and 2 use a shared Pin Change Interrupt mechanism, while Sensor 3 demonstrates the dedicated external interrupt requirement. Timer1 provides an independent two-second hardware timing event, with its own LED output to clearly distinguish it from the main safety-lock actuator.

The use of short ISRs, `volatile` flags, independent debounce timing, and the Sense-Think-Act structure keeps the system organized and non-blocking.

Once all three sensor events have been successfully detected, the main lock output is activated and remains solid ON to represent the maintained lockdown state, while the Timer1 heartbeat continues independently in the background.
