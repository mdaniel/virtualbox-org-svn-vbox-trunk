/* $Id$ */
/** @file
 * PHY MDIO unit tests.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>

#include "../DevE1000Phy.h"

/**
 * Test fixture for PHY MDIO/MDC interface emulation.
 */
class PhyTest : public CppUnit::TestFixture  {
    CPPUNIT_TEST_SUITE( PhyTest );

    CPPUNIT_TEST(testSize);
    CPPUNIT_TEST(testReadPID);
    CPPUNIT_TEST(testReadEPID);
    CPPUNIT_TEST(testWriteANA);

    CPPUNIT_TEST_SUITE_END();

private:
    enum Op
    {
        WRITE_OP = 0x1,
        READ_OP  = 0x2
    };

#define PHYADR_VAL (uint16_t)0
#define ST_VAL     (uint16_t)1
#define TA_VAL     (uint16_t)2
#define PREAMBLE_VAL 0xFFFFFFFF

    enum BitWidths {
        ST_BITS       =  2,
        OP_BITS       =  2,
        PHYADR_BITS   =  5,
        REGADR_BITS   =  5,
        TA_BITS       =  2,
        DATA_BITS     = 16,
        PREAMBLE_BITS = 32
    };

    PPHY phy;

    // Helper methods
    void shiftOutBits(uint32_t data, uint16_t count);
    uint16_t shiftInBits(uint16_t count);
    int readAt(uint16_t addr);
    void writeTo(uint16_t addr, uint32_t value);

public:
    void setUp()
    {
        phy = new PHY;
        Phy::init(phy, 0, PHY_EPID_M881000);
    }

    void tearDown()
    {
        delete phy;
    }

    void testSize()
    {
        CPPUNIT_ASSERT_EQUAL(32, ST_BITS+OP_BITS+PHYADR_BITS+REGADR_BITS+TA_BITS+DATA_BITS);
    }

    void testReadPID()
    {
        CPPUNIT_ASSERT_EQUAL(0x0141, readAt(2));
    }

    void testReadEPID()
    {
        CPPUNIT_ASSERT_EQUAL(0x0141, readAt(2));
        CPPUNIT_ASSERT_EQUAL(PHY_EPID_M881000, readAt(3));
    }

    void testWriteANA()
    {
        writeTo(4, 0xBEEF);
        CPPUNIT_ASSERT_EQUAL(0xBEEF, readAt(4));
    }

};

/**
 *  shiftOutBits - Shift data bits our to MDIO
 **/
void PhyTest::shiftOutBits(uint32_t data, uint16_t count) {
    uint32_t mask = 0x01 << (count - 1);

    do {
        Phy::writeMDIO(phy, data & mask);
        mask >>= 1;
    } while (mask);
}

/**
 *  shiftInBits - Shift data bits in from MDIO
 **/
uint16_t PhyTest::shiftInBits(uint16_t count)
{
    uint16_t data = 0;

    while (count--)
    {
        data <<= 1;
        data |= Phy::readMDIO(phy) ? 1 : 0;
    }

    return data;
}

int PhyTest::readAt(uint16_t addr)
{
    shiftOutBits(PREAMBLE_VAL, PREAMBLE_BITS);

    shiftOutBits(ST_VAL, ST_BITS);
    shiftOutBits(READ_OP, OP_BITS);
    shiftOutBits(PHYADR_VAL, PHYADR_BITS);
    shiftOutBits(addr, REGADR_BITS);

    CPPUNIT_ASSERT_EQUAL((uint16_t)0, shiftInBits(1));
    uint16_t u16Data = shiftInBits(DATA_BITS);
    shiftInBits(1);
    return u16Data;
}

void PhyTest::writeTo(uint16_t addr, uint32_t value)
{
    shiftOutBits(PREAMBLE_VAL, PREAMBLE_BITS);

    shiftOutBits(ST_VAL, ST_BITS);
    shiftOutBits(WRITE_OP, OP_BITS);
    shiftOutBits(PHYADR_VAL, PHYADR_BITS);
    shiftOutBits(addr, REGADR_BITS);
    shiftOutBits(TA_VAL, TA_BITS);
    shiftOutBits(value, DATA_BITS);
}

// Create text test runner and run all tests.
int main( int argc, char **argv)
{
    CppUnit::TextUi::TestRunner runner;
    runner.addTest( PhyTest::suite() );
    return runner.run() ? 0 : 1;
}

