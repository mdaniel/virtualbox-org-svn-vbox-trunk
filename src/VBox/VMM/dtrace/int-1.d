/* $Id$ */
/** @file
 * DTracing VBox - Interrupt Experiment #1.
 */

/*
 * Copyright (C) 2012-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#pragma D option quiet

uint64_t g_aStarts[uint32_t];
uint64_t g_cUntaggedHighs;
uint64_t g_cUntaggedGets;
uint64_t g_cMissedHighs;

inline uint32_t kfDevIdMask = 0x3ff;


/*
 * Timestamp the when the device raises the IRQ.
 */
vboxvmm*:::pdm-irq-high,vboxvmm*:::pdm-irq-hilo
/args[1] != 0/
{
    /*printf("high: tag=%#x src=%d %llx -> %llx\n", args[1], args[2], g_aStarts[args[1]], timestamp);*/
    g_aStarts[args[1]] = timestamp;
}

vboxvmm*:::pdm-irq-high,vboxvmm*:::pdm-irq-hilo
/args[1] == 0/
{
    g_cUntaggedHighs++;
}

/*
 * Catch the CPU getting the IRQ from the (A)PIC and preparing for injection.
 */
vboxvmm*:::pdm-irq-get
/g_aStarts[args[1]] == 0 && args[1] != 0/
{
    printf("get:  tag=%#x src=%d %llx - %llx = %llx\n", args[1], args[2], timestamp, g_aStarts[args[1]], timestamp - g_aStarts[args[1]]);
    @g_MissedHighs[args[3], args[2] & kfDevIdMask] = count();
    g_cMissedHighs++;
}

vboxvmm*:::pdm-irq-get
/g_aStarts[args[1]] > 0 && args[1] != 0/
{
    /*printf("get:  tag=%#x src=%d %llx - %llx = %llx\n", args[1], args[2], timestamp, g_aStarts[args[1]], timestamp - g_aStarts[args[1]]);*/
    @g_Interrupts[args[3], args[2] & kfDevIdMask] = count();
    @g_DispAvg[   args[3], args[2] & kfDevIdMask]  = avg(timestamp - g_aStarts[args[1]]);
    @g_DispMax[   args[3], args[2] & kfDevIdMask]  = max(timestamp - g_aStarts[args[1]]);
    @g_DispMin[   args[3], args[2] & kfDevIdMask]  = min(timestamp - g_aStarts[args[1]]);
    g_aStarts[args[1]] = 0;
    g_cHits++;
}

vboxvmm*:::pdm-irq-get
/args[1] == 0/
{
    @g_UntaggedGets[args[3]] = count();
    g_cUntaggedGets++;
}

vboxvmm*:::pdm-irq-get
/args[2] > kfDevIdMask/
{
    @g_Shared[args[3], args[2] & kfDevIdMask] = count();
}

/* For the time being, quit after 256 interrupts. */
vboxvmm*:::pdm-irq-get
/g_cHits >= 256/
{
    exit(0);
}

/*
 * Catch the device clearing the IRQ.
 */


/*
 * Report.
 */
END
{
    printf("\nInterrupt distribution:\n");
    printa("    irq %3d    dev %2d    %@12u\n", @g_Interrupts);
    printf("Interrupt sharing (devices detect pushing a line high at the same time):\n");
    printa("    irq %3d    dev %2d    %@12u\n", @g_Shared);
    printf("Minimum dispatch latency:\n");
    printa("    irq %3d    dev %2d    %@12u ns\n", @g_DispMin);
    printf("Average dispatch latency:\n");
    printa("    irq %3d    dev %2d    %@12u ns\n", @g_DispAvg);
    printf("Maximum dispatch latency:\n");
    printa("    irq %3d    dev %2d    %@12u ns\n", @g_DispMax);
}
END
/g_cUntaggedHighs > 0/
{
    printf("Untagged highs: %u\n", g_cUntaggedHighs);
}
END
/g_cUntaggedGets > 0/
{
    printf("Untagged gets: %u\n", g_cUntaggedGets);
    printa("    irq %3d              %@12u\n", @g_UntaggedGets);
}
END
/g_cMissedHighs > 0/
{
    printf("Missed (or shared?) highs: %u\n", g_cMissedHighs);
    printa("    irq %3d    dev %2d    %@12u\n", @g_MissedHighs);
}

