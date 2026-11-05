/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3 as
 *  published by the Free Software Foundation.
 */

#include <sblib/eib/bcu_base.h>

#include "HelperFunctions.h"
#include <sblib/eib/typesBCU2.h>

MsaComObjectLayout HelperFunctions::layout;

bool MsaComObjectLayout::object(const int objno, uint8_t& type, uint16_t& valueAddress)
{
	if (objno < 0 || objno >= capacity || entries[objno].type == UNKNOWN)
	{
		return false;
	}
	type = entries[objno].type;
	valueAddress = entries[objno].address;
	return true;
}

void MsaComObjectLayout::setObject(const int objno, const uint8_t type, const uint16_t valueAddress)
{
	if (objno < 0)
	{
		return;
	}
	if (objno >= capacity)
	{
		const int newCapacity = objno + 16;
		Entry* grown = new Entry[newCapacity];
		for (int i = 0; i < newCapacity; i++)
		{
			grown[i] = (i < capacity) ? entries[i] : Entry {0, UNKNOWN};
		}
		delete[] entries;
		entries = grown;
		capacity = newCapacity;
	}
	entries[objno] = Entry {valueAddress, type};
}

void HelperFunctions::setComObjPtr(ComObjects* bcu, int objno, ComType type, uint16_t &newAddr)
{
	uint8_t objectCount;
	if (bcu->readObjectConfigTable(0, &objectCount, 1)) // only with a loaded com object table, as before
	{
		int size = 1;
	    if (type > BYTE_1)
	    {
	    	const uint8_t objectTypeSizes[10] = { 1, 1, 2, 3, 4, 6, 8, 10, 14, 14 };
	    	size = objectTypeSizes[type - BIT_7];
	    }

	    if (newAddr + size > 0x5FC + 0x60 && newAddr < 0x6C5)
	    {
	    	newAddr = 0x6C5 + 128;
	    }

	    // before, type and data pointer were written into the com object table at every start
	    layout.setObject(objno, type, newAddr);

	    newAddr += size;
	}
}

void HelperFunctions::setFlagsTablePtr(ComObjects* bcu, uint16_t flagsAddr)
{
	uint8_t objectCount;
	if (bcu->readObjectConfigTable(0, &objectCount, 1))
	{
		layout.setFlagsTable(flagsAddr);
	}
}
