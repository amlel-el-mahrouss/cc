/* -------------------------------------------

	Copyright (C) 2024-2025 Amlal EL Mahrous, all rights reserved

	File: XCOFF.hpp
	Purpose: XCOFF for NeKernel.

	Revision History:

	04/07/24: Added file (amlel)

------------------------------------------- */

#ifndef __XCOFF__
#define __XCOFF__

#include <LibCompiler/Defines.h>

#define kXCOFF64Magic 0x01F7

#define kXCOFFRelFlg	 0x0001
#define kXCOFFExecutable 0x0002
#define kXCOFFLnno		 0x0004
#define kXCOFFLSyms		 0x0008

namespace LibCompiler
{
	/// @brief XCoff identification header.
	typedef struct XCoffFileHeader
	{
		UInt16	fMagic;
		UInt16	fTarget;
		UInt16	fNumSecs;
		UInt32	fTimeDat;
		UIntPtr fSymPtr;
		UInt32	fNumSyms;
		UInt16	fOptHdr; // ?: Number of bytes in optional header
	} XCoffFileHeader;
} // namespace LibCompiler

#endif // ifndef __XCOFF__
