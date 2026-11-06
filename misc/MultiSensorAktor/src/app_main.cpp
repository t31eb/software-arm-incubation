/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/bcu_base.h>
#include <sblib/eibMASK0701.h>
#include <sblib/eib/userEepromFlash.h>
#include <sblib/eib/userEepromSpi.h>
#include <sblib/eib/userMemoryExtension.h>
#include <sblib/spi_eeprom.h>
#include <sblib/spi.h>
#include <sblib/i2c.h>
#include <cstring>

#include <GenericItem.h>
#include <ARMPinItem.h>
#include <PCA9555DItem.h>
#include <CCS811Item.h>
#include <SHT2xItem.h>
#include <SHT4xItem.h>
#include <SGP4xItem.h>

#include <DeviceConfig.h>

#include <HelperFunctions.h>

APP_VERSION("MSA     ", "0", "16"); // Don't forget to also change the build-variable sw_version
#define CONFIG_ADDRESS 0x4800

#ifdef MSA_USER_EEPROM_SPI
// The user EEPROM in an SPI EEPROM or FRAM at SSP1 (J1/IC6), 12 MHz, chip select as GPIO (CMake option MSA_USER_EEPROM).
// Nothing is kept in RAM; the chip starts in bcu.begin(). Don't use SSP1 for anything else then.
SpiEepromBusSsp spiEepromBus(SPI_PORT_1, PIO3_2, PIO2_2, PIO2_3, PIO2_4);
SpiEeprom spiEeprom(spiEepromBus, MSA_USER_EEPROM_SPI);
UserEepromSpi userEepromSpi(spiEeprom, UserEepromMASK0701::SIZE);
MASK0701 bcu = MASK0701(&userEepromSpi);
#else
// The user EEPROM stays in the flash, only one page of 256 bytes is cached in RAM instead of a mirror of 3 KB
MASK0701 bcu = MASK0701(new UserEepromFlash(UserEepromMASK0701::SIZE));
#endif

// Digital pin for LED
#define PIO_LED PIO2_0

GenericItem *firstItem = nullptr;

const unsigned char hardwareVersion[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x48 };

// Configuration of the I2C items, segment 0x6000-0x6EFF of the product database outside of the user EEPROM
#define CONFIG_EXTENSION_START 0x6000
#define CONFIG_EXTENSION_SIZE 0xF00

#ifdef MSA_USER_EEPROM_SPI
// In the same chip as the user EEPROM, with a header of its own at 0x1000; no flash sector, no RAM buffer
UserEepromSpi configExtensionSpi(spiEeprom, CONFIG_EXTENSION_SIZE, 0x1000);
UserMemoryExtension configExtension(CONFIG_EXTENSION_START, CONFIG_EXTENSION_SIZE, &configExtensionSpi);
#else
// In the flash sector 0xE000-0xEFFF, through the MemMapper (256 bytes buffer in RAM)
MemMapper memMapper = MemMapper(0xe000, 0x1000, false);
#endif

/**
 * Copy the configuration of one item from the user EEPROM or the MemMapper into a new buffer on the heap.
 *
 * The items keep a pointer to their configuration and read it in every Loop(). They get this copy instead of a
 * pointer into the configuration memory, so after setup() nothing reads the non-volatile memory through pointers.
 * ETS restarts the device after every download, so the copy can't get stale.
 *
 * @param virtAddress Virtual (ETS) address of the configuration.
 * @param length      Number of bytes to copy.
 * @return The copy; zeros where the memory couldn't be read.
 */
static uint8_t* copyConfig(const uint32_t virtAddress, const uint32_t length)
{
    auto* copy = new uint8_t[length]();
    uint32_t readable = length;
    const uint32_t extensionEnd = CONFIG_EXTENSION_START + CONFIG_EXTENSION_SIZE;
    if ((virtAddress >= CONFIG_EXTENSION_START) && (virtAddress < extensionEnd) && (virtAddress + length > extensionEnd))
    {
        readable = extensionEnd - virtAddress; // a maximum length must not leave the range, the rest stays 0
    }
    (void) bcu.readUserMemory(virtAddress, copy, readable); // user EEPROM, MemMapper or extension
    return copy;
}

/**
 * Application setup
 */
BcuBase* setup()
{
    bcu.setHardwareType(hardwareVersion, sizeof(hardwareVersion));
    bcu.begin(0x13A, 0x01, 0x0B); // Manufacturer name "Not assigned", app-id 0x01, version 0.10

    GenericItem::comObjects = bcu.comObjects;
    GenericPin::comObjects = bcu.comObjects;

    pinMode(PIO_LED, OUTPUT);
    digitalWrite(PIO_LED, 1);

#ifdef MSA_USER_EEPROM_SPI
    bcu.setUserMemoryExtension(&configExtension);
#else
    memMapper.addRange(CONFIG_EXTENSION_START, CONFIG_EXTENSION_SIZE);
    bcu.setMemMapper(&memMapper);
#endif

    bcu.comObjects->setObjectLayout(&HelperFunctions::layout);
    HelperFunctions::setFlagsTablePtr(bcu.comObjects, 0x6C5);
    uint16_t objRamPointer = 0x5FC;

    DeviceConfig deviceConfigCopy {};
    (void) bcu.userEeprom->readBlock(CONFIG_ADDRESS, (uint8_t*)&deviceConfigCopy, sizeof(deviceConfigCopy));
    const DeviceConfig* deviceConfig = &deviceConfigCopy;
    uint8_t nextComObj = 1;
    uint32_t configAddress = CONFIG_EXTENSION_START; // configuration of the I2C items

  /*  if (deviceConfig->BusSwitches & BusSwitch::SPI0)
    {
    	SPI spi0 = SPI(SPI_PORT_0);
    	spi0.setClockDivider(120); // 400kHz
    	spi0.setDataSize(SPI_DATA_8BIT);
    	spi0.begin();

    }

    if (deviceConfig->BusSwitches & BusSwitch::SPI1)
    {
    	SPI spi0 = SPI(SPI_PORT_1);
    	spi0.setClockDivider(120); // 400kHz
    	spi0.setDataSize(SPI_DATA_8BIT);
    	spi0.begin();

    }*/

    // We need millis to be at least 1 for debouncer to work
    while (!millis());

    if (deviceConfig->BusSwitches & BusSwitch::I2C)
    {
    	i2c_lpcopen_init();
    	Chip_I2C_SetClockRate(I2C0, 400000);

    	for (int i = 0; i < deviceConfig->PCA9555DCount; i++)
    	{
    		// variable length, at most 16 pins; the item tells the real length afterwards
    		constexpr uint32_t maxLength = sizeof(PCA9555DConfig) + 16 * sizeof(PCA9555DPinConfig);
    		firstItem = new PCA9555DItem(nextComObj, (PCA9555DConfig*)copyConfig(configAddress, maxLength), firstItem,
    		                             objRamPointer);
    		nextComObj += firstItem->ComObjCount();
    		configAddress += firstItem->ConfigLength();
    	}

    	for (int i = 0; i < deviceConfig->CCS811Count; i++)
    	{
    		firstItem = new CCS811Item(nextComObj, (CCS811Config*)copyConfig(configAddress, sizeof(CCS811Config)),
    		                           firstItem, objRamPointer);
    		nextComObj += firstItem->ComObjCount();
    		configAddress += firstItem->ConfigLength();
    	}

    	if (deviceConfig->SHTOption & SHTSwitch::SHT2x)
    	{
    		firstItem = new SHT2xItem(nextComObj,
    		                      (TempHumSensorConfig*)copyConfig(configAddress, sizeof(TempHumSensorConfig)),
    		                      firstItem, objRamPointer);
    		nextComObj += firstItem->ComObjCount();
    		configAddress += firstItem->ConfigLength();
    	}

    	if (deviceConfig->SHTOption & SHTSwitch::SHT4x)
    	{
    		firstItem = new SHT4xItem(nextComObj,
    		                      (TempHumSensorConfig*)copyConfig(configAddress, sizeof(TempHumSensorConfig)),
    		                      firstItem, objRamPointer);
    		nextComObj += firstItem->ComObjCount();
    		configAddress += firstItem->ConfigLength();
    	}

    	if (deviceConfig->SHTOption & SHTSwitch::SGP4x)
    	{
    		firstItem = new SGP4xItem(nextComObj,
    		                      (TempHumSensorConfig*)copyConfig(configAddress, sizeof(TempHumSensorConfig)),
    		                      firstItem, objRamPointer);
    		nextComObj += firstItem->ComObjCount();
    		configAddress += firstItem->ConfigLength();
    	}
    }

    configAddress = CONFIG_ADDRESS + sizeof(DeviceConfig); // configuration of the ARM pins, in the user EEPROM
    unsigned int pinCount = sizeof(ARMPinItem::PortPins) / sizeof(ARMPinItem::PortPins[0]);
	for (unsigned int i = 0; i < pinCount; i++)
	{
		if (deviceConfig->PortAssignment[i] > 0 && deviceConfig->PortAssignment[i] <= pinCount)
		{
			firstItem = new ARMPinItem(nextComObj, deviceConfig->PortAssignment[i] - 1,
			                           (ARMPinConfig*)copyConfig(configAddress, sizeof(ARMPinConfig)), firstItem,
			                           objRamPointer);
			nextComObj += firstItem->ComObjCount();
			configAddress += firstItem->ConfigLength();
		}
	}

    interrupts();

    return &bcu;
}

/**
 * The application's main.
 */
void loop()
{
    // Handle updated communication objects
    int objNo = -1;

    do
    {
    	uint32_t now = millis();
        GenericItem* item = firstItem;
        while (item != nullptr)
        {
        	item->Loop(now, objNo);
        	item = item->GetNextItem();
        }
    }
    while ((objNo = bcu.comObjects->nextUpdatedObject()) >= 0);

    // Sleep up to 1 millisecond if there is nothing to do
    waitForInterrupt();
}

/**
 * The processing loop while no KNX-application is loaded
 */
void loop_noapp()
{

}
