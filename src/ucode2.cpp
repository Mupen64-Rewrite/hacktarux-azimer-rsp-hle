/*
 * Copyright (c) 2025, hacktarux-azimer-rsp-hle maintainers, contributors, and original authors (Hacktarux, Azimer).
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "stdafx.h"
#include "hle.h"

extern uint8_t BufferSpace[0x10000];

static void SPNOOP()
{
    char buff[0x100];
    sprintf(buff, "Unknown/Unimplemented Audio Command %i in ABI 2", inst1 >> 24);
    MessageBox(NULL, buff, "Audio HLE Error", MB_OK);
}

extern uint16_t AudioInBuffer; // 0x0000(T8)
extern uint16_t AudioOutBuffer; // 0x0002(T8)
extern uint16_t AudioCount; // 0x0004(T8)
extern uint32_t loopval; // 0x0010(T8)
extern uint32_t SEGMENTS[0x10];

extern uint16_t adpcmtable[0x88];

extern uint16_t ResampleLUT[0x200];

bool isMKABI = false;
bool isZeldaABI = false;

static void LOADADPCM2()
{
    // Loads an ADPCM table - Works 100% Now 03-13-01
    uint32_t v0;
    v0 = (inst2 & 0xffffff); // + SEGMENTS[(inst2>>24)&0xf];
    uint16_t* table = (uint16_t*)(rsp.rdram + v0); // Zelda2 Specific...

    for (uint32_t x = 0; x < ((inst1 & 0xffff) >> 0x4); x++)
    {
        adpcmtable[0x1 + (x << 3)] = table[0];
        adpcmtable[0x0 + (x << 3)] = table[1];

        adpcmtable[0x3 + (x << 3)] = table[2];
        adpcmtable[0x2 + (x << 3)] = table[3];

        adpcmtable[0x5 + (x << 3)] = table[4];
        adpcmtable[0x4 + (x << 3)] = table[5];

        adpcmtable[0x7 + (x << 3)] = table[6];
        adpcmtable[0x6 + (x << 3)] = table[7];
        table += 8;
    }
}

static void SETLOOP2()
{
    loopval = inst2 & 0xffffff; // No segment?
}

static void SETBUFF2()
{
    AudioInBuffer = uint16_t(inst1); // 0x00
    AudioOutBuffer = uint16_t((inst2 >> 0x10)); // 0x02
    AudioCount = uint16_t(inst2); // 0x04
}

static void ADPCM2()
{
    // Verified to be 100% Accurate...
    BYTE Flags = (uint8_t)(inst1 >> 16) & 0xff;
    WORD Gain = (uint16_t)(inst1 & 0xffff);
    DWORD Address = (inst2 & 0xffffff); // + SEGMENTS[(inst2>>24)&0xf];
    WORD inPtr = 0;
    // short *out=(int16_t *)(testbuff+(AudioOutBuffer>>2));
    short* out = (short*)(BufferSpace + AudioOutBuffer);
    BYTE* in = (BYTE*)(BufferSpace + AudioInBuffer);
    short count = (short)AudioCount;
    BYTE icode;
    BYTE code;
    int vscale;
    WORD index;
    WORD j;
    int a[8];
    short *book1, *book2;

    uint8_t srange;
    uint8_t inpinc;
    uint8_t mask1;
    uint8_t mask2;
    uint8_t shifter;

    memset(out, 0, 32);

    if (Flags & 0x4)
    {
        // Tricky lil Zelda MM and ABI2!!! hahaha I know your secrets! :DDD
        srange = 0xE;
        inpinc = 0x5;
        mask1 = 0xC0;
        mask2 = 0x30;
        shifter = 10;
    }
    else
    {
        srange = 0xC;
        inpinc = 0x9;
        mask1 = 0xf0;
        mask2 = 0x0f;
        shifter = 12;
    }

    if (!(Flags & 0x1))
    {
        if (Flags & 0x2)
        {
            /*
                        for(int i=0;i<16;i++)
                        {
                            out[i]=*(short *)&rsp.rdram[(loopval+i*2)^2];
                        }*/
            memcpy(out, &rsp.rdram[loopval], 32);
        }
        else
        {
            /*
                        for(int i=0;i<16;i++)
                        {
                            out[i]=*(short *)&rsp.rdram[(Address+i*2)^2];
                        }*/
            memcpy(out, &rsp.rdram[Address], 32);
        }
    }

    int l1 = out[15];
    int l2 = out[14];
    int inp1[8];
    int inp2[8];
    out += 16;
    while (count > 0)
    {
        code = BufferSpace[(AudioInBuffer + inPtr) ^ 3];
        index = code & 0xf;
        index <<= 4;
        book1 = (short*)&adpcmtable[index];
        book2 = book1 + 8;
        code >>= 4;
        vscale = (0x8000 >> ((srange - code) - 1));

        inPtr++;
        j = 0;

        while (j < 8)
        {
            icode = BufferSpace[(AudioInBuffer + inPtr) ^ 3];
            inPtr++;

            inp1[j] = (int16_t)((icode & mask1) << 8); // this will in effect be signed
            if (code < srange)
                inp1[j] = ((int)((int)inp1[j] * (int)vscale) >> 16);
            else
                int catchme = 1;
            j++;

            inp1[j] = (int16_t)((icode & mask2) << shifter);
            if (code < srange)
                inp1[j] = ((int)((int)inp1[j] * (int)vscale) >> 16);
            else
                int catchme = 1;
            j++;

            if (Flags & 4)
            {
                inp1[j] = (int16_t)((icode & 0xC) << 12); // this will in effect be signed
                if (code < 0xE)
                    inp1[j] = ((int)((int)inp1[j] * (int)vscale) >> 16);
                else
                    int catchme = 1;
                j++;

                inp1[j] = (int16_t)((icode & 0x3) << 14);
                if (code < 0xE)
                    inp1[j] = ((int)((int)inp1[j] * (int)vscale) >> 16);
                else
                    int catchme = 1;
                j++;
            } // end flags
        } // end while


        j = 0;
        while (j < 8)
        {
            icode = BufferSpace[(AudioInBuffer + inPtr) ^ 3];
            inPtr++;

            inp2[j] = (int16_t)((icode & mask1) << 8);
            if (code < srange)
                inp2[j] = ((int)((int)inp2[j] * (int)vscale) >> 16);
            else
                int catchme = 1;
            j++;

            inp2[j] = (int16_t)((icode & mask2) << shifter);
            if (code < srange)
                inp2[j] = ((int)((int)inp2[j] * (int)vscale) >> 16);
            else
                int catchme = 1;
            j++;

            if (Flags & 4)
            {
                inp2[j] = (int16_t)((icode & 0xC) << 12);
                if (code < 0xE)
                    inp2[j] = ((int)((int)inp2[j] * (int)vscale) >> 16);
                else
                    int catchme = 1;
                j++;

                inp2[j] = (int16_t)((icode & 0x3) << 14);
                if (code < 0xE)
                    inp2[j] = ((int)((int)inp2[j] * (int)vscale) >> 16);
                else
                    int catchme = 1;
                j++;
            } // end flags
        }

        a[0] = (int)book1[0] * (int)l1;
        a[0] += (int)book2[0] * (int)l2;
        a[0] += (int)inp1[0] * (int)2048;

        a[1] = (int)book1[1] * (int)l1;
        a[1] += (int)book2[1] * (int)l2;
        a[1] += (int)book2[0] * inp1[0];
        a[1] += (int)inp1[1] * (int)2048;

        a[2] = (int)book1[2] * (int)l1;
        a[2] += (int)book2[2] * (int)l2;
        a[2] += (int)book2[1] * inp1[0];
        a[2] += (int)book2[0] * inp1[1];
        a[2] += (int)inp1[2] * (int)2048;

        a[3] = (int)book1[3] * (int)l1;
        a[3] += (int)book2[3] * (int)l2;
        a[3] += (int)book2[2] * inp1[0];
        a[3] += (int)book2[1] * inp1[1];
        a[3] += (int)book2[0] * inp1[2];
        a[3] += (int)inp1[3] * (int)2048;

        a[4] = (int)book1[4] * (int)l1;
        a[4] += (int)book2[4] * (int)l2;
        a[4] += (int)book2[3] * inp1[0];
        a[4] += (int)book2[2] * inp1[1];
        a[4] += (int)book2[1] * inp1[2];
        a[4] += (int)book2[0] * inp1[3];
        a[4] += (int)inp1[4] * (int)2048;

        a[5] = (int)book1[5] * (int)l1;
        a[5] += (int)book2[5] * (int)l2;
        a[5] += (int)book2[4] * inp1[0];
        a[5] += (int)book2[3] * inp1[1];
        a[5] += (int)book2[2] * inp1[2];
        a[5] += (int)book2[1] * inp1[3];
        a[5] += (int)book2[0] * inp1[4];
        a[5] += (int)inp1[5] * (int)2048;

        a[6] = (int)book1[6] * (int)l1;
        a[6] += (int)book2[6] * (int)l2;
        a[6] += (int)book2[5] * inp1[0];
        a[6] += (int)book2[4] * inp1[1];
        a[6] += (int)book2[3] * inp1[2];
        a[6] += (int)book2[2] * inp1[3];
        a[6] += (int)book2[1] * inp1[4];
        a[6] += (int)book2[0] * inp1[5];
        a[6] += (int)inp1[6] * (int)2048;

        a[7] = (int)book1[7] * (int)l1;
        a[7] += (int)book2[7] * (int)l2;
        a[7] += (int)book2[6] * inp1[0];
        a[7] += (int)book2[5] * inp1[1];
        a[7] += (int)book2[4] * inp1[2];
        a[7] += (int)book2[3] * inp1[3];
        a[7] += (int)book2[2] * inp1[4];
        a[7] += (int)book2[1] * inp1[5];
        a[7] += (int)book2[0] * inp1[6];
        a[7] += (int)inp1[7] * (int)2048;

        for (j = 0; j < 8; j++)
        {
            a[j ^ 1] >>= 11;
            if (a[j ^ 1] > 32767)
                a[j ^ 1] = 32767;
            else if (a[j ^ 1] < -32768)
                a[j ^ 1] = -32768;
            *(out++) = a[j ^ 1];
        }
        l1 = a[6];
        l2 = a[7];

        a[0] = (int)book1[0] * (int)l1;
        a[0] += (int)book2[0] * (int)l2;
        a[0] += (int)inp2[0] * (int)2048;

        a[1] = (int)book1[1] * (int)l1;
        a[1] += (int)book2[1] * (int)l2;
        a[1] += (int)book2[0] * inp2[0];
        a[1] += (int)inp2[1] * (int)2048;

        a[2] = (int)book1[2] * (int)l1;
        a[2] += (int)book2[2] * (int)l2;
        a[2] += (int)book2[1] * inp2[0];
        a[2] += (int)book2[0] * inp2[1];
        a[2] += (int)inp2[2] * (int)2048;

        a[3] = (int)book1[3] * (int)l1;
        a[3] += (int)book2[3] * (int)l2;
        a[3] += (int)book2[2] * inp2[0];
        a[3] += (int)book2[1] * inp2[1];
        a[3] += (int)book2[0] * inp2[2];
        a[3] += (int)inp2[3] * (int)2048;

        a[4] = (int)book1[4] * (int)l1;
        a[4] += (int)book2[4] * (int)l2;
        a[4] += (int)book2[3] * inp2[0];
        a[4] += (int)book2[2] * inp2[1];
        a[4] += (int)book2[1] * inp2[2];
        a[4] += (int)book2[0] * inp2[3];
        a[4] += (int)inp2[4] * (int)2048;

        a[5] = (int)book1[5] * (int)l1;
        a[5] += (int)book2[5] * (int)l2;
        a[5] += (int)book2[4] * inp2[0];
        a[5] += (int)book2[3] * inp2[1];
        a[5] += (int)book2[2] * inp2[2];
        a[5] += (int)book2[1] * inp2[3];
        a[5] += (int)book2[0] * inp2[4];
        a[5] += (int)inp2[5] * (int)2048;

        a[6] = (int)book1[6] * (int)l1;
        a[6] += (int)book2[6] * (int)l2;
        a[6] += (int)book2[5] * inp2[0];
        a[6] += (int)book2[4] * inp2[1];
        a[6] += (int)book2[3] * inp2[2];
        a[6] += (int)book2[2] * inp2[3];
        a[6] += (int)book2[1] * inp2[4];
        a[6] += (int)book2[0] * inp2[5];
        a[6] += (int)inp2[6] * (int)2048;

        a[7] = (int)book1[7] * (int)l1;
        a[7] += (int)book2[7] * (int)l2;
        a[7] += (int)book2[6] * inp2[0];
        a[7] += (int)book2[5] * inp2[1];
        a[7] += (int)book2[4] * inp2[2];
        a[7] += (int)book2[3] * inp2[3];
        a[7] += (int)book2[2] * inp2[4];
        a[7] += (int)book2[1] * inp2[5];
        a[7] += (int)book2[0] * inp2[6];
        a[7] += (int)inp2[7] * (int)2048;

        for (j = 0; j < 8; j++)
        {
            a[j ^ 1] >>= 11;
            if (a[j ^ 1] > 32767)
                a[j ^ 1] = 32767;
            else if (a[j ^ 1] < -32768)
                a[j ^ 1] = -32768;
            *(out++) = a[j ^ 1];
        }
        l1 = a[6];
        l2 = a[7];

        count -= 32;
    }
    out -= 16;
    memcpy(&rsp.rdram[Address], out, 32);
}

static void CLEARBUFF2()
{
    uint16_t addr = (uint16_t)(inst1 & 0xffff);
    uint16_t count = (uint16_t)(inst2 & 0xffff);
    if (count > 0)
        memset(BufferSpace + addr, 0, count);
}

static void LOADBUFF2()
{
    // Needs accuracy verification...
    uint32_t v0;
    uint32_t cnt = (((inst1 >> 0xC) + 3) & 0xFFC);
    v0 = (inst2 & 0xfffffc); // + SEGMENTS[(inst2>>24)&0xf];
    memcpy(BufferSpace + (inst1 & 0xfffc), rsp.rdram + v0, (cnt + 3) & 0xFFFC);
}

static void SAVEBUFF2()
{
    // Needs accuracy verification...
    uint32_t v0;
    uint32_t cnt = (((inst1 >> 0xC) + 3) & 0xFFC);
    v0 = (inst2 & 0xfffffc); // + SEGMENTS[(inst2>>24)&0xf];
    memcpy(rsp.rdram + v0, BufferSpace + (inst1 & 0xfffc), (cnt + 3) & 0xFFFC);
}


static void MIXER2()
{
    // Needs accuracy verification...
    uint16_t dmemin = (uint16_t)(inst2 >> 0x10);
    uint16_t dmemout = (uint16_t)(inst2 & 0xFFFF);
    uint32_t count = ((inst1 >> 12) & 0xFF0);
    int32_t gain = (int16_t)(inst1 & 0xFFFF) * 2;
    int32_t temp;

    for (int x = 0; x < count; x += 2)
    {
        // I think I can do this a lot easier

        temp = (*(int16_t*)(BufferSpace + dmemin + x) * gain) >> 16;
        temp += *(int16_t*)(BufferSpace + dmemout + x);

        if ((int32_t)temp > 32767)
            temp = 32767;
        if ((int32_t)temp < -32768)
            temp = -32768;

        *(uint16_t*)(BufferSpace + dmemout + x) = (uint16_t)(temp & 0xFFFF);
    }
}


static void RESAMPLE2()
{
    BYTE Flags = (uint8_t)((inst1 >> 16) & 0xff);
    DWORD Pitch = ((inst1 & 0xffff)) << 1;
    uint32_t addy = (inst2 & 0xffffff); // + SEGMENTS[(inst2>>24)&0xf];
    DWORD Accum = 0;
    DWORD location;
    int16_t* lut;
    short* dst;
    int16_t* src;
    dst = (short*)(BufferSpace);
    src = (int16_t*)(BufferSpace);
    uint32_t srcPtr = (AudioInBuffer / 2);
    uint32_t dstPtr = (AudioOutBuffer / 2);
    int32_t temp;
    int32_t accum;

    if (addy > (1024 * 1024 * 8))
        addy = (inst2 & 0xffffff);

    srcPtr -= 4;

    if ((Flags & 0x1) == 0)
    {
        for (int x = 0; x < 4; x++) // memcpy (src+srcPtr, rsp.rdram+addy, 0x8);
            src[(srcPtr + x) ^ 1] = ((uint16_t*)rsp.rdram)[((addy / 2) + x) ^ 1];
        Accum = *(uint16_t*)(rsp.rdram + addy + 10);
    }
    else
    {
        for (int x = 0; x < 4; x++)
            src[(srcPtr + x) ^ 1] = 0; //*(uint16_t *)(rsp.rdram+((addy+x)^2));
    }

    //	if ((Flags & 0x2))
    //		__asm int 3;

    for (int i = 0; i < ((AudioCount + 0xf) & 0xFFF0) / 2; i++)
    {
        location = (((Accum * 0x40) >> 0x10) * 8);
        // location = (Accum >> 0xa) << 0x3;
        lut = (int16_t*)(((uint8_t*)ResampleLUT) + location);

        temp = ((int32_t) * (int16_t*)(src + ((srcPtr + 0) ^ 1)) * ((int32_t)((int16_t)lut[0])));
        accum = (int32_t)(temp >> 15);

        temp = ((int32_t) * (int16_t*)(src + ((srcPtr + 1) ^ 1)) * ((int32_t)((int16_t)lut[1])));
        accum += (int32_t)(temp >> 15);

        temp = ((int32_t) * (int16_t*)(src + ((srcPtr + 2) ^ 1)) * ((int32_t)((int16_t)lut[2])));
        accum += (int32_t)(temp >> 15);

        temp = ((int32_t) * (int16_t*)(src + ((srcPtr + 3) ^ 1)) * ((int32_t)((int16_t)lut[3])));
        accum += (int32_t)(temp >> 15);

        if (accum > 32767)
            accum = 32767;
        if (accum < -32768)
            accum = -32768;

        dst[dstPtr ^ 1] = (int16_t)(accum);
        dstPtr++;
        Accum += Pitch;
        srcPtr += (Accum >> 16);
        Accum &= 0xffff;
    }
    for (int x = 0; x < 4; x++)
        ((uint16_t*)rsp.rdram)[((addy / 2) + x) ^ 1] = src[(srcPtr + x) ^ 1];
    *(uint16_t*)(rsp.rdram + addy + 10) = (uint16_t)Accum;
    // memcpy (RSWORK, src+srcPtr, 0x8);
}

static void DMEMMOVE2()
{
    // Needs accuracy verification...
    uint32_t v0, v1;
    uint32_t cnt;
    if ((inst2 & 0xffff) == 0)
        return;
    v0 = (inst1 & 0xFFFF);
    v1 = (inst2 >> 0x10);
    // assert ((v1 & 0x3) == 0);
    // assert ((v0 & 0x3) == 0);
    uint32_t count = ((inst2 + 3) & 0xfffc);
    // v0 = (v0) & 0xfffc;
    // v1 = (v1) & 0xfffc;

    // memcpy (dmem+v1, dmem+v0, count-1);
    for (cnt = 0; cnt < count; cnt++)
    {
        *(uint8_t*)(BufferSpace + ((cnt + v1) ^ 3)) = *(uint8_t*)(BufferSpace + ((cnt + v0) ^ 3));
    }
}

uint32_t t3, s5, s6;
uint16_t env[8];

static void ENVSETUP1()
{
    uint32_t tmp;

    // fprintf (dfile, "ENVSETUP1: inst1 = %08X, inst2 = %08X\n", inst1, inst2);
    t3 = inst1 & 0xFFFF;
    tmp = (inst1 >> 0x8) & 0xFF00;
    env[4] = (uint16_t)tmp;
    tmp += t3;
    env[5] = (uint16_t)tmp;
    s5 = inst2 >> 0x10;
    s6 = inst2 & 0xFFFF;
    // fprintf (dfile, "	t3 = %X / s5 = %X / s6 = %X / env[4] = %X / env[5] = %X\n", t3, s5, s6, env[4], env[5]);
}

static void ENVSETUP2()
{
    uint32_t tmp;

    // fprintf (dfile, "ENVSETUP2: inst1 = %08X, inst2 = %08X\n", inst1, inst2);
    tmp = (inst2 >> 0x10);
    env[0] = (uint16_t)tmp;
    tmp += s5;
    env[1] = (uint16_t)tmp;
    tmp = inst2 & 0xffff;
    env[2] = (uint16_t)tmp;
    tmp += s6;
    env[3] = (uint16_t)tmp;
    // fprintf (dfile, "	env[0] = %X / env[1] = %X / env[2] = %X / env[3] = %X\n", env[0], env[1], env[2], env[3]);
}

static void ENVMIXER2()
{
    // fprintf (dfile, "ENVMIXER: inst1 = %08X, inst2 = %08X\n", inst1, inst2);

    int16_t *bufft6, *bufft7, *buffs0, *buffs1;
    int16_t* buffs3;
    int32_t count;
    uint32_t adder;

    int16_t vec9, vec10;

    int16_t v2[8];

    //__asm int 3;

    buffs3 = (int16_t*)(BufferSpace + ((inst1 >> 0x0c) & 0x0ff0));
    bufft6 = (int16_t*)(BufferSpace + ((inst2 >> 0x14) & 0x0ff0));
    bufft7 = (int16_t*)(BufferSpace + ((inst2 >> 0x0c) & 0x0ff0));
    buffs0 = (int16_t*)(BufferSpace + ((inst2 >> 0x04) & 0x0ff0));
    buffs1 = (int16_t*)(BufferSpace + ((inst2 << 0x04) & 0x0ff0));


    v2[0] = 0 - (int16_t)((inst1 & 0x2) >> 1);
    v2[1] = 0 - (int16_t)((inst1 & 0x1));
    v2[2] = 0 - (int16_t)((inst1 & 0x8) >> 1);
    v2[3] = 0 - (int16_t)((inst1 & 0x4) >> 1);

    count = (inst1 >> 8) & 0xff;

    if (!isMKABI)
    {
        s5 *= 2;
        s6 *= 2;
        t3 *= 2;
        adder = 0x10;
    }
    else
    {
        inst1 = 0;
        adder = 0x8;
        t3 = 0;
    }


    while (count > 0)
    {
        int temp, x;
        for (x = 0; x < 0x8; x++)
        {
            vec9 = (int16_t)(((int32_t)buffs3[x ^ 1] * (uint32_t)env[0]) >> 0x10) ^ v2[0];
            vec10 = (int16_t)(((int32_t)buffs3[x ^ 1] * (uint32_t)env[2]) >> 0x10) ^ v2[1];
            temp = bufft6[x ^ 1] + vec9;
            if (temp > 32767)
                temp = 32767;
            if (temp < -32768)
                temp = -32768;
            bufft6[x ^ 1] = temp;
            temp = bufft7[x ^ 1] + vec10;
            if (temp > 32767)
                temp = 32767;
            if (temp < -32768)
                temp = -32768;
            bufft7[x ^ 1] = temp;
            vec9 = (int16_t)(((int32_t)vec9 * (uint32_t)env[4]) >> 0x10) ^ v2[2];
            vec10 = (int16_t)(((int32_t)vec10 * (uint32_t)env[4]) >> 0x10) ^ v2[3];
            if (inst1 & 0x10)
            {
                temp = buffs0[x ^ 1] + vec10;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                buffs0[x ^ 1] = temp;
                temp = buffs1[x ^ 1] + vec9;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                buffs1[x ^ 1] = temp;
            }
            else
            {
                temp = buffs0[x ^ 1] + vec9;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                buffs0[x ^ 1] = temp;
                temp = buffs1[x ^ 1] + vec10;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                buffs1[x ^ 1] = temp;
            }
        }

        if (!isMKABI)
            for (x = 0x8; x < 0x10; x++)
            {
                vec9 = (int16_t)(((int32_t)buffs3[x ^ 1] * (uint32_t)env[1]) >> 0x10) ^ v2[0];
                vec10 = (int16_t)(((int32_t)buffs3[x ^ 1] * (uint32_t)env[3]) >> 0x10) ^ v2[1];
                temp = bufft6[x ^ 1] + vec9;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                bufft6[x ^ 1] = temp;
                temp = bufft7[x ^ 1] + vec10;
                if (temp > 32767)
                    temp = 32767;
                if (temp < -32768)
                    temp = -32768;
                bufft7[x ^ 1] = temp;
                vec9 = (int16_t)(((int32_t)vec9 * (uint32_t)env[5]) >> 0x10) ^ v2[2];
                vec10 = (int16_t)(((int32_t)vec10 * (uint32_t)env[5]) >> 0x10) ^ v2[3];
                if (inst1 & 0x10)
                {
                    temp = buffs0[x ^ 1] + vec10;
                    if (temp > 32767)
                        temp = 32767;
                    if (temp < -32768)
                        temp = -32768;
                    buffs0[x ^ 1] = temp;
                    temp = buffs1[x ^ 1] + vec9;
                    if (temp > 32767)
                        temp = 32767;
                    if (temp < -32768)
                        temp = -32768;
                    buffs1[x ^ 1] = temp;
                }
                else
                {
                    temp = buffs0[x ^ 1] + vec9;
                    if (temp > 32767)
                        temp = 32767;
                    if (temp < -32768)
                        temp = -32768;
                    buffs0[x ^ 1] = temp;
                    temp = buffs1[x ^ 1] + vec10;
                    if (temp > 32767)
                        temp = 32767;
                    if (temp < -32768)
                        temp = -32768;
                    buffs1[x ^ 1] = temp;
                }
            }
        bufft6 += adder;
        bufft7 += adder;
        buffs0 += adder;
        buffs1 += adder;
        buffs3 += adder;
        count -= adder;
        env[0] += (uint16_t)s5;
        env[1] += (uint16_t)s5;
        env[2] += (uint16_t)s6;
        env[3] += (uint16_t)s6;
        env[4] += (uint16_t)t3;
        env[5] += (uint16_t)t3;
    }
}

static void DUPLICATE2()
{
    WORD Count = (inst1 >> 16) & 0xff;
    WORD In = inst1 & 0xffff;
    WORD Out = (inst2 >> 16);

    WORD buff[64];

    memcpy(buff, BufferSpace + In, 128);

    while (Count)
    {
        memcpy(BufferSpace + Out, buff, 128);
        Out += 128;
        Count--;
    }
}

/*
static void INTERL2 () { // Make your own...
    short Count = inst1 & 0xffff;
    WORD  Out   = inst2 & 0xffff;
    WORD In     = (inst2 >> 16);

    short *src,*dst,tmp;
    src=(short *)&BufferSpace[In];
    dst=(short *)&BufferSpace[Out];
    while(Count)
    {
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        *(dst++)=*(src++);
        src++;
        Count-=8;
    }
}
*/

static void INTERL2()
{
    short Count = inst1 & 0xffff;
    WORD Out = inst2 & 0xffff;
    WORD In = (inst2 >> 16);

    BYTE *src, *dst, tmp;
    src = (BYTE*)(BufferSpace); //[In];
    dst = (BYTE*)(BufferSpace); //[Out];
    while (Count)
    {
        *(short*)(dst + (Out ^ 3)) = *(short*)(src + (In ^ 3));
        Out += 2;
        In += 4;
        Count--;
    }
}

static void INTERLEAVE2()
{
    // Needs accuracy verification...
    uint32_t inL, inR;
    uint16_t* outbuff;
    uint16_t* inSrcR;
    uint16_t* inSrcL;
    uint16_t Left, Right;
    uint32_t count;
    count = ((inst1 >> 12) & 0xFF0);
    if (count == 0)
    {
        outbuff = (uint16_t*)(AudioOutBuffer + BufferSpace);
        count = AudioCount;
    }
    else
    {
        outbuff = (uint16_t*)((inst1 & 0xFFFF) + BufferSpace);
    }

    inR = inst2 & 0xFFFF;
    inL = (inst2 >> 16) & 0xFFFF;

    inSrcR = (uint16_t*)(BufferSpace + inR);
    inSrcL = (uint16_t*)(BufferSpace + inL);

    for (uint32_t x = 0; x < (count / 4); x++)
    {
        Left = *(inSrcL++);
        Right = *(inSrcR++);

        *(outbuff++) = *(inSrcR++);
        *(outbuff++) = *(inSrcL++);
        *(outbuff++) = (uint16_t)Right;
        *(outbuff++) = (uint16_t)Left;
    }
}

static void ADDMIXER()
{
    short Count = (inst1 >> 12) & 0x00ff0;
    uint16_t InBuffer = (inst2 >> 16);
    uint16_t OutBuffer = inst2 & 0xffff;

    int16_t *inp, *outp;
    int32_t temp;
    inp = (int16_t*)(BufferSpace + InBuffer);
    outp = (int16_t*)(BufferSpace + OutBuffer);
    for (int cntr = 0; cntr < Count; cntr += 2)
    {
        temp = *outp + *inp;
        if (temp > 32767)
            temp = 32767;
        if (temp < -32768)
            temp = -32768;
        outp++;
        inp++;
    }
}

static void HILOGAIN()
{
    uint16_t cnt = inst1 & 0xffff;
    uint16_t out = (inst2 >> 16) & 0xffff;
    int16_t hi = (int16_t)((inst1 >> 4) & 0xf000);
    uint16_t lo = (inst1 >> 20) & 0xf;
    int16_t* src;

    src = (int16_t*)(BufferSpace + out);
    int32_t tmp, val;

    while (cnt)
    {
        val = (int32_t)*src;
        // tmp = ((val * (int32_t)hi) + ((uint64_t)(val * lo) << 16) >> 16);
        tmp = ((val * (int32_t)hi) >> 16) + (uint32_t)(val * lo);
        if ((int32_t)tmp > 32767)
            tmp = 32767;
        else if ((int32_t)tmp < -32768)
            tmp = -32768;
        *src = tmp;
        src++;
        cnt -= 2;
    }
}

static void FILTER2()
{
    static int cnt = 0;
    static int16_t* lutt6;
    static int16_t* lutt5;
    uint8_t* save = (rsp.rdram + (inst2 & 0xFFFFFF));
    uint8_t t4 = (uint8_t)((inst1 >> 0x10) & 0xFF);
    int x;

    if (t4 > 1)
    {
        // Then set the cnt variable
        cnt = (inst1 & 0xFFFF);
        lutt6 = (int16_t*)save;
        //				memcpy (dmem+0xFE0, rsp.rdram+(inst2&0xFFFFFF), 0x10);
        return;
    }

    if (t4 == 0)
    {
        //				memcpy (dmem+0xFB0, rsp.rdram+(inst2&0xFFFFFF), 0x20);
        lutt5 = (short*)(save + 0x10);
    }

    lutt5 = (short*)(save + 0x10);

    //			lutt5 = (short *)(dmem + 0xFC0);
    //			lutt6 = (short *)(dmem + 0xFE0);
    for (x = 0; x < 8; x++)
    {
        int32_t a;
        a = (lutt5[x] + lutt6[x]) >> 1;
        lutt5[x] = lutt6[x] = (short)a;
    }
    short *inp1, *inp2;
    int32_t out1[8];
    int16_t outbuff[0x3c0], *outp;
    uint32_t inPtr = (uint32_t)(inst1 & 0xffff);
    inp1 = (short*)(save);
    outp = outbuff;
    inp2 = (short*)(BufferSpace + inPtr);
    for (x = 0; x < cnt; x += 0x10)
    {
        out1[1] = inp1[0] * lutt6[6];
        out1[1] += inp1[3] * lutt6[7];
        out1[1] += inp1[2] * lutt6[4];
        out1[1] += inp1[5] * lutt6[5];
        out1[1] += inp1[4] * lutt6[2];
        out1[1] += inp1[7] * lutt6[3];
        out1[1] += inp1[6] * lutt6[0];
        out1[1] += inp2[1] * lutt6[1]; // 1

        out1[0] = inp1[3] * lutt6[6];
        out1[0] += inp1[2] * lutt6[7];
        out1[0] += inp1[5] * lutt6[4];
        out1[0] += inp1[4] * lutt6[5];
        out1[0] += inp1[7] * lutt6[2];
        out1[0] += inp1[6] * lutt6[3];
        out1[0] += inp2[1] * lutt6[0];
        out1[0] += inp2[0] * lutt6[1];

        out1[3] = inp1[2] * lutt6[6];
        out1[3] += inp1[5] * lutt6[7];
        out1[3] += inp1[4] * lutt6[4];
        out1[3] += inp1[7] * lutt6[5];
        out1[3] += inp1[6] * lutt6[2];
        out1[3] += inp2[1] * lutt6[3];
        out1[3] += inp2[0] * lutt6[0];
        out1[3] += inp2[3] * lutt6[1];

        out1[2] = inp1[5] * lutt6[6];
        out1[2] += inp1[4] * lutt6[7];
        out1[2] += inp1[7] * lutt6[4];
        out1[2] += inp1[6] * lutt6[5];
        out1[2] += inp2[1] * lutt6[2];
        out1[2] += inp2[0] * lutt6[3];
        out1[2] += inp2[3] * lutt6[0];
        out1[2] += inp2[2] * lutt6[1];

        out1[5] = inp1[4] * lutt6[6];
        out1[5] += inp1[7] * lutt6[7];
        out1[5] += inp1[6] * lutt6[4];
        out1[5] += inp2[1] * lutt6[5];
        out1[5] += inp2[0] * lutt6[2];
        out1[5] += inp2[3] * lutt6[3];
        out1[5] += inp2[2] * lutt6[0];
        out1[5] += inp2[5] * lutt6[1];

        out1[4] = inp1[7] * lutt6[6];
        out1[4] += inp1[6] * lutt6[7];
        out1[4] += inp2[1] * lutt6[4];
        out1[4] += inp2[0] * lutt6[5];
        out1[4] += inp2[3] * lutt6[2];
        out1[4] += inp2[2] * lutt6[3];
        out1[4] += inp2[5] * lutt6[0];
        out1[4] += inp2[4] * lutt6[1];

        out1[7] = inp1[6] * lutt6[6];
        out1[7] += inp2[1] * lutt6[7];
        out1[7] += inp2[0] * lutt6[4];
        out1[7] += inp2[3] * lutt6[5];
        out1[7] += inp2[2] * lutt6[2];
        out1[7] += inp2[5] * lutt6[3];
        out1[7] += inp2[4] * lutt6[0];
        out1[7] += inp2[7] * lutt6[1];

        out1[6] = inp2[1] * lutt6[6];
        out1[6] += inp2[0] * lutt6[7];
        out1[6] += inp2[3] * lutt6[4];
        out1[6] += inp2[2] * lutt6[5];
        out1[6] += inp2[5] * lutt6[2];
        out1[6] += inp2[4] * lutt6[3];
        out1[6] += inp2[7] * lutt6[0];
        out1[6] += inp2[6] * lutt6[1];
        outp[1] = /*CLAMP*/ ((out1[1] + 0x4000) >> 0xF);
        outp[0] = /*CLAMP*/ ((out1[0] + 0x4000) >> 0xF);
        outp[3] = /*CLAMP*/ ((out1[3] + 0x4000) >> 0xF);
        outp[2] = /*CLAMP*/ ((out1[2] + 0x4000) >> 0xF);
        outp[5] = /*CLAMP*/ ((out1[5] + 0x4000) >> 0xF);
        outp[4] = /*CLAMP*/ ((out1[4] + 0x4000) >> 0xF);
        outp[7] = /*CLAMP*/ ((out1[7] + 0x4000) >> 0xF);
        outp[6] = /*CLAMP*/ ((out1[6] + 0x4000) >> 0xF);
        inp1 = inp2;
        inp2 += 8;
        outp += 8;
    }
    //			memcpy (rsp.rdram+(inst2&0xFFFFFF), dmem+0xFB0, 0x20);
    memcpy(save, inp2 - 8, 0x10);
    memcpy(BufferSpace + (inst1 & 0xffff), outbuff, cnt);
}

static void SEGMENT2()
{
    if (isZeldaABI)
    {
        FILTER2();
        return;
    }
    if ((inst1 & 0xffffff) == 0)
    {
        isMKABI = true;
        // SEGMENTS[(inst2>>24)&0xf] = (inst2 & 0xffffff);
    }
    else
    {
        isMKABI = false;
        isZeldaABI = true;
        FILTER2();
    }
}

static void UNKNOWN()
{
}

/*
void (*ABI2[0x20])() = {
    SPNOOP, ADPCM2, CLEARBUFF2, SPNOOP, SPNOOP, RESAMPLE2, SPNOOP, SEGMENT2,
    SETBUFF2, SPNOOP, DMEMMOVE2, LOADADPCM2, MIXER2, INTERLEAVE2, HILOGAIN, SETLOOP2,
    SPNOOP, INTERL2, ENVSETUP1, ENVMIXER2, LOADBUFF2, SAVEBUFF2, ENVSETUP2, SPNOOP,
    SPNOOP, SPNOOP, SPNOOP, SPNOOP, SPNOOP, SPNOOP, SPNOOP, SPNOOP
};*/

void (*ABI2[0x20])() = {
SPNOOP, ADPCM2, CLEARBUFF2, UNKNOWN, ADDMIXER, RESAMPLE2, UNKNOWN, SEGMENT2,
SETBUFF2, DUPLICATE2, DMEMMOVE2, LOADADPCM2, MIXER2, INTERLEAVE2, HILOGAIN, SETLOOP2,
SPNOOP, INTERL2, ENVSETUP1, ENVMIXER2, LOADBUFF2, SAVEBUFF2, ENVSETUP2, SPNOOP,
HILOGAIN, SPNOOP, DUPLICATE2, UNKNOWN, SPNOOP, SPNOOP, SPNOOP, SPNOOP};
/*
void (*ABI2[0x20])() = {
    SPNOOP , ADPCM2, CLEARBUFF2, SPNOOP, SPNOOP, RESAMPLE2  , SPNOOP  , SEGMENT2,
    SETBUFF2 , DUPLICATE2, DMEMMOVE2, LOADADPCM2, MIXER2, INTERLEAVE2, SPNOOP, SETLOOP2,
    SPNOOP, INTERL2 , ENVSETUP1, ENVMIXER2, LOADBUFF2, SAVEBUFF2, ENVSETUP2, SPNOOP,
    SPNOOP , SPNOOP, SPNOOP , SPNOOP    , SPNOOP  , SPNOOP    , SPNOOP  , SPNOOP
};
/* NOTES:

  FILTER/SEGMENT - Still needs to be finished up... add FILTER?
  UNKNOWWN #27	 - Is this worth doing?  Looks like a pain in the ass just for WaveRace64
*/
