/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2025 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

/**
 * This demo program is designed to test the USB MIDI Host driver for a single USB
 * MIDI device connected to the USB Host port or up to 4 devices connected via a
 * USB hub. It sends to each USB MIDI device the sequence of half-steps from B-flat
 * to D whose note numbers correspond to the transport button LEDs on a Mackie Control
 * compatible control surface. It also prints to a UART serial port console the messages
 * received from each USB MIDI device.
 *
 * If you wish to use the built in USB serial port that Arduino supports,
 * change the "#define OUTPUT Serial1" line below to "#define OUTPUT Serial"
 * However, during testing, I have noted that the software locks up randomly
 * in response to receiving MIDI IN messages if OUTPUT is Serial instead of Serial1.
 */
#include <Arduino.h>
#if defined(USE_TINYUSB_HOST) || !defined(USE_TINYUSB)
#error "Please use the Menu to select Tools->USB Stack: Adafruit TinyUSB"
#endif
#include "pio_usb.h"
#define HOST_PIN_DP   16   // Pin used as D+ for host, D- = D+ + 1
#include "EZ_USB_MIDI_HOST.h"
// USB Host object
Adafruit_USBH_Host USBHost;

USING_NAMESPACE_MIDI
USING_NAMESPACE_EZ_USB_MIDI_HOST
struct mycustomsettings : public MidiHostSettingsDefault
{
    static const unsigned MidiRxBufsize = 512;
};

RPPICOMIDI_EZ_USB_MIDI_HOST_INSTANCE(usbhMIDI, mycustomsettings)
#define OUTPUT Serial1
/* MIDI IN MESSAGE REPORTING */
static void onMidiError(int8_t errCode)
{
    OUTPUT.printf("MIDI Errors: %s %s %s\r\n", (errCode & (1UL << ErrorParse)) ? "Parse":"",
        (errCode & (1UL << ErrorActiveSensingTimeout)) ? "Active Sensing Timeout" : "",
        (errCode & (1UL << WarningSplitSysEx)) ? "Split SysEx":"");
}

static void printAddrAndCable()
{
    uint8_t midiDevAddr, cable;
    usbhMIDI.getCurrentReadDevAndCable(midiDevAddr, cable);
    OUTPUT.printf("[%02d,%02d] ",midiDevAddr, cable);
}

static void onNoteOff(Channel channel, byte note, byte velocity)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: Note off#%u v=%u\r\n", channel, note, velocity);
}

static void onNoteOn(Channel channel, byte note, byte velocity)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: Note on#%u v=%u\r\n", channel, note, velocity);
}

static void onPolyphonicAftertouch(Channel channel, byte note, byte amount)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: PAT#%u=%u\r\n", channel, note, amount);
}

static void onControlChange(Channel channel, byte controller, byte value)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: CC#%u=%u\r\n", channel, controller, value);
}

static void onProgramChange(Channel channel, byte program)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: Prog=%u\r\n", channel, program);
}

static void onAftertouch(Channel channel, byte value)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: AT=%u\r\n", channel, value);
}

static void onPitchBend(Channel channel, int value)
{
    printAddrAndCable();
    OUTPUT.printf("C%u: PB=%d\r\n", channel, value);
}

static void onSysEx(byte * array, unsigned size)
{
    printAddrAndCable();
    OUTPUT.printf("SysEx:\r\n");
    unsigned multipleOf8 = size/8;
    unsigned remOf8 = size % 8;
    for (unsigned idx=0; idx < multipleOf8; idx++) {
        for (unsigned jdx = 0; jdx < 8; jdx++) {
            OUTPUT.printf("%02x ", *array++);
        }
        OUTPUT.printf("\r\n");
    }
    for (unsigned idx = 0; idx < remOf8; idx++) {
        OUTPUT.printf("%02x ", *array++);
    }
    OUTPUT.printf("\r\n");
}

static void onSMPTEqf(byte data)
{
    printAddrAndCable();
    uint8_t type = (data >> 4) & 0xF;
    data &= 0xF;    
    static const char* fps[4] = {"24", "25", "30DF", "30ND"};
    switch (type) {
        case 0: OUTPUT.printf("SMPTE FRM LS %u \r\n", data); break;
        case 1: OUTPUT.printf("SMPTE FRM MS %u \r\n", data); break;
        case 2: OUTPUT.printf("SMPTE SEC LS %u \r\n", data); break;
        case 3: OUTPUT.printf("SMPTE SEC MS %u \r\n", data); break;
        case 4: OUTPUT.printf("SMPTE MIN LS %u \r\n", data); break;
        case 5: OUTPUT.printf("SMPTE MIN MS %u \r\n", data); break;
        case 6: OUTPUT.printf("SMPTE HR LS %u \r\n", data); break;
        case 7:
            OUTPUT.printf("SMPTE HR MS %u FPS:%s\r\n", data & 0x1, fps[(data >> 1) & 3]);
            break;
        default:
          OUTPUT.printf("invalid SMPTE data byte %u\r\n", data);
          break;
    }
}

static void onSongPosition(unsigned beats)
{
    printAddrAndCable();
    OUTPUT.printf("SongP=%u\r\n", beats);
}

static void onSongSelect(byte songnumber)
{
    printAddrAndCable();
    OUTPUT.printf("SongS#%u\r\n", songnumber);
}

static void onTuneRequest()
{
    printAddrAndCable();
    OUTPUT.printf("Tune\r\n");
}

static void onMidiClock()
{
    printAddrAndCable();
    OUTPUT.printf("Clock\r\n");
}

static void onMidiStart()
{
    printAddrAndCable();
    OUTPUT.printf("Start\r\n");
}

static void onMidiContinue()
{
    printAddrAndCable();
    OUTPUT.printf("Cont\r\n");
}

static void onMidiStop()
{
    printAddrAndCable();
    OUTPUT.printf("Stop\r\n");
}

static void onActiveSense()
{
    printAddrAndCable();
    OUTPUT.printf("ASen\r\n");
}

static void onSystemReset()
{
    printAddrAndCable();
    OUTPUT.printf("SysRst\r\n");
}

static void onMidiTick()
{
    printAddrAndCable();
    OUTPUT.printf("Tick\r\n");
}

static void onMidiInWriteFail(uint8_t devAddr, uint8_t cable, bool fifoOverflow)
{
    if (fifoOverflow)
        OUTPUT.printf("[%02d,%02d] MIDI IN FIFO overflow\r\n", devAddr, cable);
    else
        OUTPUT.printf("[%02d,%02d] MIDI IN FIFO error\r\n", devAddr, cable);
}

static void registerMidiInCallbacks(uint8_t midiDevAddr)
{
    uint8_t ncables = usbhMIDI.getNumInCables(midiDevAddr);
    for (uint8_t cable = 0; cable < ncables; cable++) {
        auto intf = usbhMIDI.getInterfaceFromDeviceAndCable(midiDevAddr, cable);
        intf->setHandleNoteOff(onNoteOff);                      // 0x80
        intf->setHandleNoteOn(onNoteOn);                        // 0x90
        intf->setHandleAfterTouchPoly(onPolyphonicAftertouch);  // 0xA0
        intf->setHandleControlChange(onControlChange);          // 0xB0
        intf->setHandleProgramChange(onProgramChange);          // 0xC0
        intf->setHandleAfterTouchChannel(onAftertouch);         // 0xD0
        intf->setHandlePitchBend(onPitchBend);                  // 0xE0
        intf->setHandleSystemExclusive(onSysEx);                // 0xF0, 0xF7
        intf->setHandleTimeCodeQuarterFrame(onSMPTEqf);         // 0xF1
        intf->setHandleSongPosition(onSongPosition);            // 0xF2
        intf->setHandleSongSelect(onSongSelect);                // 0xF3
        intf->setHandleTuneRequest(onTuneRequest);              // 0xF6
        intf->setHandleClock(onMidiClock);                      // 0xF8
        // 0xF9 as 10ms Tick is not MIDI 1.0 standard but implemented in the Arduino MIDI Library
        intf->setHandleTick(onMidiTick);                        // 0xF9
        intf->setHandleStart(onMidiStart);                      // 0xFA
        intf->setHandleContinue(onMidiContinue);                // 0xFB
        intf->setHandleStop(onMidiStop);                        // 0xFC
        intf->setHandleActiveSensing(onActiveSense);            // 0xFE
        intf->setHandleSystemReset(onSystemReset);              // 0xFF
        intf->setHandleError(onMidiError);
    }
    auto dev = usbhMIDI.getDevFromDevAddr(midiDevAddr);
    if (dev == nullptr)
        return;
    dev->setOnMidiInWriteFail(onMidiInWriteFail);
}

static void unregisterMidiInCallbacks(uint8_t midiDevAddr)
{
    uint8_t ncables = usbhMIDI.getNumInCables(midiDevAddr);
    for (uint8_t cable = 0; cable < ncables; cable++) {
        auto intf = usbhMIDI.getInterfaceFromDeviceAndCable(midiDevAddr, cable);
        if (intf == nullptr)
            return;
        intf->disconnectCallbackFromType(NoteOn);
        intf->disconnectCallbackFromType(NoteOff);
        intf->disconnectCallbackFromType(AfterTouchPoly);
        intf->disconnectCallbackFromType(ControlChange);
        intf->disconnectCallbackFromType(ProgramChange);
        intf->disconnectCallbackFromType(AfterTouchChannel);
        intf->disconnectCallbackFromType(PitchBend);
        intf->disconnectCallbackFromType(SystemExclusive);
        intf->disconnectCallbackFromType(TimeCodeQuarterFrame);
        intf->disconnectCallbackFromType(SongPosition);
        intf->disconnectCallbackFromType(SongSelect);
        intf->disconnectCallbackFromType(TuneRequest);
        intf->disconnectCallbackFromType(Clock);
        // 0xF9 as 10ms Tick is not MIDI 1.0 standard but implemented in the Arduino MIDI Library
        intf->disconnectCallbackFromType(Tick);
        intf->disconnectCallbackFromType(Start);
        intf->disconnectCallbackFromType(Continue);
        intf->disconnectCallbackFromType(Stop);
        intf->disconnectCallbackFromType(ActiveSensing);
        intf->disconnectCallbackFromType(SystemReset);
        intf->setHandleError(nullptr);
    }
    auto dev = usbhMIDI.getDevFromDevAddr(midiDevAddr);
    if (dev == nullptr)
        return;
    dev->setOnMidiInWriteFail(nullptr);
}

/* CONNECTION MANAGEMENT */
static void listConnectedDevices()
{
    OUTPUT.printf("Dev  VID:PID  Product Name[Manufacter]{serial string}\r\n");
    for (uint8_t midiDevAddr = 1; midiDevAddr <= RPPICOMIDI_TUH_MIDI_MAX_DEV; midiDevAddr++) {
        auto dev = usbhMIDI.getDevFromDevAddr(midiDevAddr);
        if (dev) {
            OUTPUT.printf("%02u  %04x:%04x %s[%s]{%s}\r\n",midiDevAddr, dev->getVID(), dev->getPID(),
                dev->getProductStr(), dev->getManufacturerStr(), dev->getSerialString());
        }
    }
}
static void onMIDIconnect(uint8_t devAddr, uint8_t nInCables, uint8_t nOutCables)
{
    OUTPUT.printf("MIDI device at address %u has %u IN cables and %u OUT cables\r\n", devAddr, nInCables, nOutCables);
    registerMidiInCallbacks(devAddr);
    listConnectedDevices();
}

static void onMIDIdisconnect(uint8_t devAddr)
{
    OUTPUT.printf("MIDI device at address %u unplugged\r\n", devAddr);
    unregisterMidiInCallbacks(devAddr);
    // Note that listConnectedDevices() will still list the just unplugged
    //  device as connected until this function returns
    listConnectedDevices();
}

/* MAIN LOOP FUNCTIONS */

static void blinkLED(void)
{
    const uint32_t intervalMs = 1000;
    static uint32_t startMs = 0;

    static bool ledState = false;
    if ( millis() - startMs < intervalMs)
        return;
    startMs += intervalMs;

    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH:LOW); 
}

static void sendNextNote()
{
    static uint8_t firstNote = 0x5b; // Mackie Control rewind
    static uint8_t lastNote = 0x5f; // Mackie Control stop
    static uint8_t offNote = lastNote;
    static uint8_t onNote = firstNote;
    const uint32_t intervalMs = 1000;
    static uint32_t startMs = 0;
    if (millis() - startMs < intervalMs)
        return; // not enough time
    startMs += intervalMs;
    for (uint8_t midiDevAddr = 1; midiDevAddr <= RPPICOMIDI_TUH_MIDI_MAX_DEV; midiDevAddr++) {
        auto intf = usbhMIDI.getInterfaceFromDeviceAndCable(midiDevAddr, usbhMIDI.getNumOutCables(midiDevAddr)-1);
        if (intf == nullptr)
            continue; // not connected
        intf->sendNoteOn(offNote, 0, 1);
        intf->sendNoteOn(onNote, 0x7f, 1);
        
    }
    if (++offNote > lastNote)
        offNote = firstNote;
    if (++onNote > lastNote)
        onNote = firstNote;
}

/* APPLICATION STARTS HERE */
void setup()
{
    OUTPUT.begin(115200);

    while(!OUTPUT);   // wait for serial port
    pinMode(LED_BUILTIN, OUTPUT);
    // Check for CPU frequency, must be multiple of 120Mhz for bit-banging USB
    uint32_t cpu_hz = clock_get_hz(clk_sys);
    if ( cpu_hz != 120000000UL && cpu_hz != 240000000UL ) {
        delay(2000);   // wait for native usb
        OUTPUT.printf("Error: CPU Clock = %u, PIO USB require CPU clock must be multiple of 120 Mhz\r\n", cpu_hz);
        OUTPUT.printf("Change your CPU Clock to either 120 or 240 Mhz in Menu->CPU Speed \r\n", cpu_hz);
        while(1) delay(1);
    }

    pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
    pio_cfg.pin_dp = HOST_PIN_DP;
  
    USBHost.configure_pio_usb(1, &pio_cfg);
    usbhMIDI.begin(&USBHost, 1, onMIDIconnect, onMIDIdisconnect);
    OUTPUT.println("EZ USB MIDI HOST PIO Example for Arduino\r\n");
}

void loop() {
    // Handle USB Stack processing
    USBHost.task();
    // Handle any incoming data; triggers MIDI IN callbacks
    usbhMIDI.readAll();
    // Do other processing that might generate pending MIDI OUT data
    sendNextNote();

    // Tell the USB Host to send as much pending MIDI OUT data as possible
    usbhMIDI.writeFlushAll();

    // Do other non-USB host processing
    blinkLED();
}

