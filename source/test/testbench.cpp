/*****************************************************************************
 * Copyright (C) 2013 x265 project
 *
 * Authors: Gopu Govindaswamy <gopu@govindaswamy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@multicorewareinc.com.
 *****************************************************************************/

#include "unittest.h"
#include "primitives.h"
#include <time.h>
#include <iostream>
#include <ctype.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Code snippet from http://www.winehq.org/pipermail/wine-devel/2003-June/018082.html begins
// this part is windows implementation of Gettimeoffday() function

#ifndef _TIMEVAL_H
#define _TIMEVAL_H

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <time.h>

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif
__inline int gettimeofday(struct timeval *tv,  struct timezone *tz)
{
    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;

    if (tv)
    {
        GetSystemTimeAsFileTime(&ft);
        li.LowPart  = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        t  = li.QuadPart;       /* In 100-nanosecond intervals */
        t -= EPOCHFILETIME;     /* Offset to the Epoch time */
        t /= 10;                /* In microseconds */
        tv->tv_sec  = (long)(t / 1000000);
        tv->tv_usec = (long)(t % 1000000);
    }

    return 0;
}

#else  /* _WIN32 */

#include <sys/time.h>

#endif /* _WIN32 */

#endif /* _TIMEVAL_H */

// Code snippet from http://www.winehq.org/pipermail/wine-devel/2003-June/018082.html ends

using namespace x265;

/* pbuf1, pbuf2: initialized to random pixel data and shouldn't write into them. */
pixel *pbuf1, *pbuf2;
uint16_t do_singleprimitivecheck = 0;
uint16_t curpar = 0, cycletest = 0, cycletest_primitive = 0;
#define BENCH_ALIGNS 16

// Initialize the Func Names for all the Pixel Comp
static const char *FuncNames[NUM_PARTITIONS] = {
"4x4", "8x4", "4x8", "8x8", "4x16", "16x4", "8x16", "16x8", "16x16", "4x32", "32x4", "8x32",
"32x8", "16x32", "32x16", "32x32", "4x64", "64x4", "8x64", "64x8", "16x64", "64x16", "32x64", "64x32", "64x64"
};

#if HIGH_BIT_DEPTH
#define PIXEL_MAX ((1 << 10) - 1)
#else
#define PIXEL_MAX ((1 << 8) - 1)
#endif

static double timevaldiff(struct timeval *starttime, struct timeval *finishtime)
{
    double msec;

    msec =  (finishtime->tv_sec - starttime->tv_sec) * 1000;
    msec += (double)(finishtime->tv_usec - starttime->tv_usec) / 1000;
    return msec;
}

static void check_cycle_count(pixelcmp cprimitive, pixelcmp opt)
{
    struct timeval ts, te;
    const int num_iterations = 100000;

    // prime the cache
    opt(pbuf1, 16, pbuf2, 16);

    gettimeofday(&ts, NULL);
    for (int j = 0; j < num_iterations; j++)
        opt(pbuf1, 16, pbuf2, 16);
    gettimeofday(&te, NULL);
    printf("\tvectorized: (%1.4f ms) ", timevaldiff(&ts, &te));

    gettimeofday(&ts, NULL);
    for (int j = 0; j < num_iterations; j++)
        cprimitive(pbuf1, 16, pbuf2, 16);
    gettimeofday(&te, NULL);
    printf("\tC: (%1.4f ms) %d iterations\n", timevaldiff(&ts, &te), num_iterations);
}

static int check_pixelprimitive(pixelcmp ref, pixelcmp opt)
{
    int j = 0;
    for (int i = 0; i <= 100; i++)
    {
        int vres = opt(pbuf1 + j, 16, pbuf2, 16);
        int cres = ref(pbuf1 + j, 16, pbuf2, 16);
        if (vres != cres)
            return -1;

        j += 16;
    }
    return 0;
}

// test all implemented pixel comparison primitives
static int check_pixelprimitives(const EncoderPrimitives& cprimitives, const EncoderPrimitives& vectorprimitives)
{
    for (; curpar < NUM_PARTITIONS; curpar++)
    {
        if (vectorprimitives.satd[curpar])
        {
            if (check_pixelprimitive(cprimitives.satd[curpar], vectorprimitives.satd[curpar]) < 0)
            {
                printf("satd[%s]: failed!\n", FuncNames[curpar]);
                return -1;
            }
            printf("satd[%s]: passed ", FuncNames[curpar]);
            check_cycle_count(cprimitives.satd[curpar], vectorprimitives.satd[curpar]);
        }
        if (vectorprimitives.sad[curpar])
        {
            if (check_pixelprimitive(cprimitives.sad[curpar], vectorprimitives.sad[curpar]) < 0)
            {
                printf("sad[%s]: failed!\n", FuncNames[curpar]);
                return -1;
            }
            printf("sad[%s]: passed ", FuncNames[curpar]);
            check_cycle_count(cprimitives.sad[curpar], vectorprimitives.sad[curpar]);
        }

        if (do_singleprimitivecheck == 1)
            return 0;
    }

    if (vectorprimitives.sa8d_8x8)
    {
        if (check_pixelprimitive(cprimitives.sa8d_8x8, vectorprimitives.sa8d_8x8) < 0)
        {
            printf("sa8d_8x8: failed!\n");
            return -1;
        }
        printf("sa8d_8x8: passed ");
        check_cycle_count(cprimitives.sa8d_8x8, vectorprimitives.sa8d_8x8);
    }

    if (vectorprimitives.sa8d_16x16)
    {
        if (check_pixelprimitive(cprimitives.sa8d_16x16, vectorprimitives.sa8d_16x16) < 0)
        {
            printf("sa8d_16x16: failed!\n");
            return -1;
        }
        printf("sa8d_16x16: passed ");
        check_cycle_count(cprimitives.sa8d_16x16, vectorprimitives.sa8d_16x16);
    }

    return 0;
}

static int check_all_funcs(const EncoderPrimitives& cprimitives, const EncoderPrimitives& vectorprimitives)
{
    return check_pixelprimitives(cprimitives, vectorprimitives);
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int cpuid = CpuIDDetect();

    for (int i = 1; i < argc - 1; i += 2)
    {
        if (!strcmp(argv[i], "--primitive"))
        {
            do_singleprimitivecheck = 1;
            curpar = atoi(argv[i + 1]);
        }
        else if (!strcmp(argv[i], "--cpuid"))
        {
            cpuid = atoi(argv[i + 1]);
        }
    }

    pbuf1 = (pixel*)malloc(0x1e00 * sizeof(pixel) + 16 * BENCH_ALIGNS);
    pbuf2 = (pixel*)malloc(0x1e00 * sizeof(pixel) + 16 * BENCH_ALIGNS);
    if (!pbuf1 || !pbuf2)
    {
        fprintf(stderr, "malloc failed, unable to initiate tests!\n");
        return -1;
    }

    for (int i = 0; i < 0x1e00; i++)
    {
        //Generate the Random Buffer for Testing
        pbuf1[i] = rand() & PIXEL_MAX;
        pbuf2[i] = rand() & PIXEL_MAX;
    }

    EncoderPrimitives cprim;
    Setup_C_Primitives(cprim);

    EncoderPrimitives vecprim;
    memset(&vecprim, 0, sizeof(vecprim));
    EncoderPrimitives asmprim;
    memset(&asmprim, 0, sizeof(asmprim));

#if ENABLE_VECTOR_PRIMITIVES
    Setup_Vector_Primitives(vecprim, cpuid);
    printf("Testing vector class primitives\n");
    ret = check_all_funcs(cprim, vecprim);
    if (ret)
    {
        fprintf(stderr, "x265: at least vector primitive has failed. Go and fix that Right Now!\n");
        return -1;
    }

#endif

#if ENABLE_ASM_PRIMITIVES
    Setup_Vector_Primitives(asmprim, cpuid);
    printf("Testing assembly primitives\n");
    ret = check_all_funcs(cprim, asmprim);
    if (ret)
    {
        fprintf(stderr, "x265: at least assembly primitive has failed. Go and fix that Right Now!\n");
        return -1;
    }

#endif

    fprintf(stderr, "x265: All tests passed Yeah :)\n");

    return 0;
}
