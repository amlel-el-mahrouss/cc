/* -------------------------------------------

	Copyright (C) 2024 Theater Quality Incorporated., all rights reserved

	File: XCOFF.hpp
	Purpose: XCOFF for NewOS.

	Revision History:

	04/07/24: Added file (amlel)

------------------------------------------- */

#ifndef __XCOFF__
#define __XCOFF__

#include <ToolchainKit/Defines.h>

#define kXCOFF64Magic 0x01F7

#define kXCOFFRelFlg	 0x0001
#define kXCOFFExecutable 0x0002
#define kXCOFFLnno		 0x0004
#define kXCOFFLSyms		 0x0008

namespace ToolchainKit
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
} // namespace ToolchainKit

#endif // ifndef __XCOFF__
