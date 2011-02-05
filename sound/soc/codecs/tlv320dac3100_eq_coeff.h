/*
 * linux/sound/soc/codecs/tlv320dac3100_eq_coeff.h
 *
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 *
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * History:
 *
 * Rev 0.1 	Created the file to hold the EQ coefficient values	26-Jul-2010 
 *
 * Rev 0.2      Updated the header file with the EQ Values provided by TI
 *                                                                     29-Jul-2010
 * Rev 0.3      Updated the header file with the default ALLPASS EQ Values
 *              provided by TI for Headset Configuration	       02-Aug-2010
 */

#ifndef _TLV320DAC3100_EQ_COEFF_H
#define _TLV320DAC3100_EQ_COEFF_H


typedef char * string;

/* @struct dac3100_control
 * 
 * Used to maintain the information about the Coefficients and its Locations.
 * This structure will be useful when we are working with the coefficients
 * in Non-Adaptive Mode. Also, the structure will be useful when there
 * are several Coefficient related configurations to be supported.
 */
typedef struct {
	u8 control_page;			// coefficient page location
	u8 control_base;			// coefficient base address within page
	u8 control_mute_flag;		// non-zero means muting required
	u8 control_string_index;	// string table indes
}dac3100_control;

/* @struct reg_value
 *
 * Structure derived from the PPS GDE which is used to represent
 * Register Offset and its Value as a pair.
 */

typedef struct {
    u8 reg_off;
    u8 reg_val;
} reg_value;


static dac3100_control	MUX_dac3100_controls[] = {
{8, 2, 1, 0},
{9, 2, 1, 1}
};


static string MUX_dac3100_control_names[] = {
"Stereo_Mux_1",
"Stereo_Mux_2"
};

/*
 * miniDSP_D_reg_Values
 * B&N Speaker EQ biquad values.
 * These are only the bytes required to setup the biquads.
 * Note that only the biaquads from 1:63 are utilized and hence
 * Configuration into Page 8 and 12 are sufficient for B&N
 * Please note page 8 will contain the Speaker EQ Configuration 
 * and Page 12 will contain the default ALLPASS Filter EQ
 * Configuration for the Headset.
 */
reg_value miniDSP_D_reg_values[] = {
    {  0,0x08},
    {  2,0x7A},
    {  3,0xF0},
    {  4,0x85},
    {  5,0x10},
    {  6,0x7A},
    {  7,0xF0},
    {  8,0x7A},
    {  9,0xD7},
    { 10,0x89},
    { 11,0xEB},
    { 12,0x7A},
    { 13,0xF0},
    { 14,0x85},
    { 15,0x10},
    { 16,0x7A},
    { 17,0xF0},
    { 18,0x7A},
    { 19,0xD7},
    { 20,0x89},
    { 21,0xEB},
    { 22,0x7E},
    { 23,0x34},
    { 24,0xA1},
    { 25,0x8B},
    { 26,0x75},
    { 27,0x85},
    { 28,0x5E},
    { 29,0x75},
    { 30,0x8C},
    { 31,0x46},
    { 32,0x7F},
    { 33,0x73},
    { 34,0x91},
    { 35,0x71},
    { 36,0x7A},
    { 37,0x6B},
    { 38,0x6F},
    { 39,0xAD},
    { 40,0x83},
    { 41,0x9A},
    { 42,0x68},
    { 43,0xB1},
    { 44,0xF9},
    { 45,0x55},
    { 46,0x49},
    { 47,0xDC},
    { 48,0x06},
    { 49,0xAB},
    { 50,0xCD},
    { 51,0x72},
    { 52,0x5C},
    { 53,0xFF},
    { 54,0xB2},
    { 55,0x35},
    { 56,0x42},
    { 57,0xE8},
    { 58,0x6E},
    { 59,0x55},
    { 60,0x9F},
    { 61,0x03},
/********** UPDATED DEFAULT ALLPASS EQ Values from here ***************/
    {  0,0x0C},
    {  2,0x7F},
    {  3,0xFF},
    {  4,0x00},
    {  5,0x00},
    {  6,0x00},
    {  7,0x00},
    {  8,0x00},
    {  9,0x00},
    { 10,0x00},
    { 11,0x00},
    { 12,0x7F},
    { 13,0xFF},
    { 14,0x00},
    { 15,0x00},
    { 16,0x00},
    { 17,0x00},
    { 18,0x00},
    { 19,0x00},
    { 20,0x00},
    { 21,0x00},
    { 22,0x7F},
    { 23,0xFF},
    { 24,0x00},
    { 25,0x00},
    { 26,0x00},
    { 27,0x00},
    { 28,0x00},
    { 29,0x00},
    { 30,0x00},
    { 31,0x00},
    { 32,0x7F},
    { 33,0xFF},
    { 34,0x00},
    { 35,0x00},
    { 36,0x00},
    { 37,0x00},
    { 38,0x00},
    { 39,0x00},
    { 40,0x00},
    { 41,0x00},
    { 42,0x7F},
    { 43,0xFF},
    { 44,0x00},
    { 45,0x00},
    { 46,0x00},
    { 47,0x00},
    { 48,0x00},
    { 49,0x00},
    { 50,0x00},
    { 51,0x00},
    { 52,0x7F},
    { 53,0xFF},
    { 54,0x00},
    { 55,0x00},
    { 56,0x00},
    { 57,0x00},
    { 58,0x00},
    { 59,0x00},
    { 60,0x00},
    { 61,0x00}

};


#endif /* #ifdef _TLV320DAC3100_EQ_COEFF_H */
