#ifndef CONFIG_H_
#define CONFIG_H_

#include <FlexCAN.h>

struct FILTER {  //should be 10 bytes
	uint32_t id;
	uint32_t mask;
	boolean extended;
	boolean enabled;
};

enum FILEOUTPUTTYPE
{
	NONE = 0,
	BINARYFILE = 1,
	GVRET = 2,
	CRTD = 3
};

struct EEPROMSettings { //Must stay under 256 - currently somewhere around 222
	uint8_t version;
	
	uint32_t Can0Speed;
	uint32_t Can1Speed;
	boolean Can0_Enabled;
	boolean Can1_Enabled;
	FILTER Can0Filters[NUM_MAILBOXES]; // filters for our 16 mailboxes - 10*8 = 160 bytes
	FILTER Can1Filters[NUM_MAILBOXES]; // filters for our 16 mailboxes - 10*8 = 160 bytes

	boolean useBinarySerialComm; //use a binary protocol on the serial link or human readable format?
	FILEOUTPUTTYPE fileOutputType; //what format should we use for file output?

	char fileNameBase[30]; //Base filename to use
	char fileNameExt[4]; //extension to use
	uint16_t fileNum; //incrementing value to append to filename if we create a new file each time
	boolean appendFile; //start a new file every power up or append to current?
	boolean autoStartLogging; //should logging start immediately on start up?

	uint8_t logLevel; //Level of logging to output on serial line
	uint8_t sysType; //0 = Teensy 3.1/3.2/3.5       1 = Teensy 3.6
	
	boolean Can0ListenOnly; //if true we don't allow any messing with the bus but rather just passively monitor.
  boolean Can1ListenOnly;

	uint16_t valid; //stores a validity token to make sure EEPROM is not corrupt
};

struct DigitalCANToggleSettings //16 bytes
{
    /* Mode is a bitfield. 
     * Bit 0 - 
     *     0 = Read pin and send message when it changes state
     *     1 = Set digital I/O on CAN Rx (Add 127
     * 
     * Bit 1 - 
     *     0 = Don't listen to or send on Can0
     *     1 = Listen on or send on Can0
     * Bit 2 -
     *     0 = Don't listen to or send on Can1
     *     1 = Listen on or send on Can1
     * Bit 7 -
     *     0 = Pin is defaulted to LOW. If bit 0 is 0 then we assume the start up state is LOW, if bit 0 is 1 then we set pin LOW
     *     1 = Pin is defaulted HIGH. If bit 0 is 0 then assume start up state is HIGH, if bit 0 is 1 then set pin HIGH
     * 
     * Mostly people don't have to worry about any of this because the serial console takes care of these details for you.
    */    
    uint8_t mode;
    uint8_t pin; //which pin we'll be using to either read a digital input or send one
    uint32_t rxTxID; //which ID to use for reception and trasmission
    uint8_t payload[8];
    uint8_t length; //how many bytes to use for the message (TX) or how many to validate (RX)
    boolean enabled; //true or false, is this special mode enabled or not?
};

struct SystemSettings 
{
	uint8_t Can0EnablePin;
	uint8_t Can1EnablePin;
	boolean useSD; //should we attempt to use the SDCard? (No logging possible otherwise)
	boolean logToFile; //are we currently supposed to be logging to file?
	uint8_t SDCardSelPin;
	boolean SDCardInserted;
	uint8_t LED_CANTX;
	uint8_t LED_CANRX;
	uint8_t LED_LOGGING;
	boolean txToggle; //LED toggle values
	boolean rxToggle;
	boolean logToggle;
	boolean lawicelMode;
	boolean lawicelAutoPoll;
	boolean lawicelTimestamping;
	int lawicelPollCounter;
};

extern EEPROMSettings settings;
extern SystemSettings SysSettings;
extern DigitalCANToggleSettings digToggleSettings;

//buffer size for SDCard - Sending canbus data to the card. 
//This is a large buffer but the sketch may as well use up a lot of RAM. It's there.
#define	SD_BUFF_SIZE	32768

//size to use for buffering writes to the USB bulk endpoint
#define SER_BUFF_SIZE		4096

//maximum number of microseconds between flushes to the USB port. 
//The host should be polling every 1ms or so and so this time should be a small multiple of that
#define SER_BUFF_FLUSH_INTERVAL	2000   

#define CFG_BUILD_NUM	400
#define CFG_VERSION "TeensyRET alpha 2016-11-21"
#define EEPROM_ADDRESS  0
#define EEPROM_VER		0x10

#define Can0_EN_PIN		2
#define Can1_EN_PIN		35
#define BLE_RST         7
#define BLE_DFU         8
#define BLE_IRQ         9

#define BLINK_LED       13

#endif /* CONFIG_H_ */
