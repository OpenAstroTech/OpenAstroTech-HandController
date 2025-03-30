#pragma once

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------
//#define BOARD_DEVICE_RHPORT_NUM     0
#define BOARD_DEVICE_RHPORT_SPEED   OPT_MODE_HIGH_SPEED

#define CFG_TUD_ENABLED             1
#define CFG_TUSB_MCU				OPT_MCU_RP2040
#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE      (OPT_MODE_HOST | BOARD_DEVICE_RHPORT_SPEED)

#define CFG_TUD_ENDPOINT0_SIZE    64

//------------- CLASS -------------//
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

// MIDI FIFO size of TX and RX
#define CFG_TUD_MIDI_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MIDI_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)


// Enable host stack
#define CFG_TUH_ENABLED 1

// Enable CDC (Communication Device Class) for both device and host
#define CFG_TUD_CDC 1
#define CFG_TUH_CDC 1

// Define the number of CDC interfaces
#define CFG_TUD_CDC_RX_BUFSIZE 64
#define CFG_TUD_CDC_TX_BUFSIZE 64
#define CFG_TUH_CDC_RX_BUFSIZE 64
#define CFG_TUH_CDC_TX_BUFSIZE 64

