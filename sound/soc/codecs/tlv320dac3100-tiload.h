/*
 * linux/sound/soc/codecs/tlv320dac3101-tiload.h
 *
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
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
 * 
 *
 * 
 */

#ifndef _TLV320DAC3101_TILOAD_H
#define _TLV320DAC3101_TILOAD_H

/* defines */

#define DAC3100_IOC_MAGIC	0xE0
#define DAC3100_IOMAGICNUM_GET	_IOR(DAC3100_IOC_MAGIC, 1, int)
#define DAC3100_IOMAGICNUM_SET	_IOW(DAC3100_IOC_MAGIC, 2, int)

#endif
