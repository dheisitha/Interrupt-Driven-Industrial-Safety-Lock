// Interrupt-Driven Industrial Safety Lock System

// Pin Definitions 
const int sensorPin1 = 8;      // Digital Sensor 1 (PB0) - PCINT0 
const int sensorPin2 = 9;      // Digital Sensor 2 (PB1) - PCINT1 
const int sensorPin3 = 2;      // Digital Sensor 3 (INT0) - External Interrupt
const int ledPin = 13;         // Actuator Output (PB5) 
const int timerLedPin = 12;    // Timer1 Periodic Task LED

// System States / Flags (Volatile because they change asynchronously inside ISRs) 
volatile uint8_t portBHistory = 0xFF;         // Tracks previous state of Port B 
volatile bool sensor1Triggered = false;       // Flag for Sensor 1 hardware event 
volatile bool sensor2Triggered = false;       // Flag for Sensor 2 hardware event 
volatile bool sensor3Triggered = false;       // Flag for Sensor 3 hardware event
volatile bool timerTickOccurred = false;      // Flag for periodic hardware Timer1 event 
volatile bool sensor1Changed = false;          // Flag for Sensor 1 PCI change
volatile bool sensor2Changed = false;          // Flag for Sensor 2 PCI change

// Serial Tracking Flags (Prevents rapid monitor printing/spam) 
bool sensor1Reported = false; 
bool sensor2Reported = false; 
bool sensor3Reported = false;
bool thinkReported = false; 

// Debounce time window in milliseconds 
const unsigned long DEBOUNCE_DELAY = 50;

// Separate debounce timestamps for each PCI sensor
unsigned long sensor1LastTime = 0;
unsigned long sensor2LastTime = 0;

// Function Prototypes 
void initializeSystem(); 
void configureInterrupts(); 
void senseInputs(); 
void thinkLogic(); 
void actOutputs(); 
void sensor3ISR();

void setup() 
{ 
    initializeSystem(); 
    configureInterrupts(); 
} 
 
void loop() 
{ 
    // Execution pipeline structured explicitly around Sense-Think-Act 
    senseInputs(); 
    thinkLogic(); 
    actOutputs(); 
} 
 
// ==================== INITIALIZATION ==================== 
void initializeSystem() 
{ 
    Serial.begin(9600); 
     
    // Configure input pins with internal pull-up resistors 
    pinMode(sensorPin1, INPUT_PULLUP); 
    pinMode(sensorPin2, INPUT_PULLUP); 
    pinMode(sensorPin3, INPUT_PULLUP);
    pinMode(ledPin, OUTPUT); 
    pinMode(timerLedPin, OUTPUT);

    // Take an initial reading of the Port B input register 
    portBHistory = PINB;  
 
    Serial.println("=================================================="); 
    Serial.println("QP4 System Active: PCI & Timer1 Hardware Congruence"); 
    Serial.println("=================================================="); 
} 
 
// Configure low-level registers for both PCINT and Timer1 
void configureInterrupts() 
{ 
    cli(); // Disable global interrupts during register configuration 
 
    // ---- 1. Pin Change Interrupt Configuration (Port B) ---- 
    // Enable Pin Change Interrupt Mask 0 (PCIE0), governing Port B (Pins 8-13) 
    PCICR |= (1 << PCIE0); 
    // Enable specific pins within PCMSK0 to trigger the interrupt 
    // PCINT0 corresponds to Arduino Pin 8 
    // PCINT1 corresponds to Arduino Pin 9 
    PCMSK0 |= (1 << PCINT0) | (1 << PCINT1); 

    // ---- 2. External Interrupt Configuration (Sensor 3) ----
    // Sensor 3 uses INT0 on Arduino Pin 2
    attachInterrupt(digitalPinToInterrupt(sensorPin3), sensor3ISR, FALLING);
 
    // ---- 3. Timer1 Hardware Configuration (CTC Mode - 2-Second Period) ---- 
    TCCR1A = 0; // Reset entire Timer1 Control Register A 
    TCCR1B = 0; // Reset entire Timer1 Control Register B 
    TCNT1  = 0; // Initialize Counter value to 0 
 
    // Set Compare Match value for a 2-second interval: 
    // Formula: OCR1A = (16,000,000 Hz / (Prescaler * Target Frequency)) - 1 
    // For 2 seconds (0.5 Hz frequency) with 1024 Prescaler: 
    // OCR1A = (16,000,000 / (1024 * 0.5)) - 1 = 31249 
    OCR1A = 31249; 
 
    // Turn on CTC (Clear Timer on Compare Match) mode 
    TCCR1B |= (1 << WGM12); 
    // Set CS12 and CS10 bits for 1024 prescaler 
    TCCR1B |= (1 << CS12) | (1 << CS10);   
    // Enable Timer1 Compare Match A Interrupt 
    TIMSK1 |= (1 << OCIE1A); 
 
    sei(); // Re-enable global interrupts 
} 
 
// ==================== HARDWARE ISR VECTORS ==================== 
 
// Vector 1: Asynchronous Event-Driven Pin Change Interrupt (Port B) 
ISR(PCINT0_vect) 
{ 
    // Read current state of entire Port B register directly 
    uint8_t currentPortB = PINB; 
    // Bitwise XOR isolates which bits flipped 
    uint8_t changedBits = currentPortB ^ portBHistory; 
 
    // Record which PCI pin changed; debounce is handled in the main loop
    if (changedBits & (1 << PB0)) 
    { 
        sensor1Changed = true; 
    } 

    if (changedBits & (1 << PB1)) 
    { 
        sensor2Changed = true; 
    } 
 
    portBHistory = currentPortB; 
} 
 
// Vector 2: Synchronous Time-Driven Timer1 Compare Match Interrupt 
ISR(TIMER1_COMPA_vect) 
{ 
    // High-safety interrupt practice: merely raise a flag
    timerTickOccurred = true; 
}

// Vector 3: External Interrupt for Sensor 3
void sensor3ISR()
{
    // High-safety interrupt practice: merely raise a flag
    sensor3Triggered = true;
}
 
// ==================== 1. SENSE FUNCTION ==================== 
void senseInputs() 
{ 
    // Handle Event-Driven telemetry (Printed exactly once per event) 
    if (sensor1Changed) 
    { 
        sensor1Changed = false;

        if (millis() - sensor1LastTime > DEBOUNCE_DELAY)
        {
            if (!(PINB & (1 << PB0)))
            {
                sensor1Triggered = true;
            }

            sensor1LastTime = millis();
        }
    }

    if (sensor2Changed) 
    { 
        sensor2Changed = false;

        if (millis() - sensor2LastTime > DEBOUNCE_DELAY)
        {
            if (!(PINB & (1 << PB1)))
            {
                sensor2Triggered = true;
            }

            sensor2LastTime = millis();
        }
    }

    if (sensor1Triggered && !sensor1Reported) 
    { 
        Serial.println("[SENSE-EVENT] PCINT Vector 0 -> Pin 8 (Sensor 1) Pressed."); 
        sensor1Reported = true; 
    } 

    if (sensor2Triggered && !sensor2Reported) 
    { 
        Serial.println("[SENSE-EVENT] PCINT Vector 0 -> Pin 9 (Sensor 2) Pressed."); 
        sensor2Reported = true; 
    }

    if (sensor3Triggered && !sensor3Reported)
    {
        Serial.println("[SENSE-EVENT] External Interrupt -> Pin 2 (Sensor 3) Pressed.");
        sensor3Reported = true;
    }
 
    // Handle Time-Driven telemetry 
    if (timerTickOccurred) 
    { 
        Serial.println("[SENSE-TIME] Timer1 Tick! 2-Second Interval Reached."); 
        // Clear flag immediately so it prints exactly once per tick 
        timerTickOccurred = false;  
         
        // Periodic background task using a separate LED
        digitalWrite(timerLedPin, !digitalRead(timerLedPin)); 
        Serial.println("[ACT-TIME] Periodic Heartbeat LED Toggled."); 
    } 
} 
 
// ==================== 2. THINK FUNCTION ==================== 
void thinkLogic() 
{ 
    // Grouped logic evaluation printed exactly once when criteria is met 
    if (sensor1Triggered && sensor2Triggered && sensor3Triggered && !thinkReported) 
    { 
        Serial.println("[THINK] Security Verification: All three sensor flags verified active."); 
        thinkReported = true; 
    } 
} 
 
// ==================== 3. ACT FUNCTION ==================== 
void actOutputs() 
{ 
    if (sensor1Triggered && sensor2Triggered && sensor3Triggered) 
    { 
        // System execution override 
        digitalWrite(ledPin, HIGH); 
        Serial.println("[ACT-EVENT] System Main Lock Engaged -> LED SOLID ON"); 
         
        // Reset state space entirely 
        sensor1Triggered = false; 
        sensor2Triggered = false; 
        sensor3Triggered = false;
        sensor1Reported = false; 
        sensor2Reported = false; 
        sensor3Reported = false;
        thinkReported = false; 
        Serial.println("[SYSTEM] Operations complete. Sensor events cleared. LOCKDOWN REMAINS ACTIVE.\n"); 
    } 
}
