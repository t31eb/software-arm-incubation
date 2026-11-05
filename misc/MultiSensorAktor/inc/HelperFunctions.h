/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#ifndef HELPERFUNCTIONS_H_
#define HELPERFUNCTIONS_H_

#include <sblib/eib/com_objects.h>

/**
 * Type and RAM address of the com objects as the MultiSensorAktor assigns them at runtime.
 *
 * The types depend on the channel configuration, the values lie one after another from 0x5FC on. The layout is
 * kept here in RAM and handed to the sblib with ComObjects::setObjectLayout(); the com object table stays as
 * ETS wrote it, so a start writes nothing into the non-volatile memory.
 */
class MsaComObjectLayout : public ComObjectLayout
{
public:
	bool object(int objno, uint8_t& type, uint16_t& valueAddress) override;
	uint16_t flagsTableAddress() override { return flagsTable; }

	void setObject(int objno, uint8_t type, uint16_t valueAddress);
	void setFlagsTable(uint16_t address) { flagsTable = address; }

private:
	static constexpr uint8_t UNKNOWN = 0xFF; //!< type of an entry the application didn't set

	struct Entry
	{
		uint16_t address;
		uint8_t type;
	};

	Entry* entries = nullptr; //!< indexed by object number, grows with the highest object number set
	int capacity = 0;
	uint16_t flagsTable = 0;
};

class HelperFunctions {
public:
	static void setComObjPtr(ComObjects* bcu, int objno, ComType type, uint16_t& addr);
	static void setFlagsTablePtr(ComObjects* bcu, uint16_t flagsAddr);

	static MsaComObjectLayout layout; //!< registered with ComObjects::setObjectLayout() in setup()
};

#endif /* HELPERFUNCTIONS_H_ */
