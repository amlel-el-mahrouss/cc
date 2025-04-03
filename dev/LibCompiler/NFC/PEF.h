/* -------------------------------------------

	Copyright (C) 2024-2025 Amlal EL Mahrous, all rights reserved

------------------------------------------- */

#pragma once

#include <LibCompiler/Defines.h>

// @file PEF.hpp
// @brief Preferred Executable Format

#define kPefMagic	 "Joy!"
#define kPefMagicFat "yoJ!"

#define kPefExt		  ".exec"
#define kPefDylibExt  ".dylib"
#define kPefLibExt	  ".lib"
#define kPefObjectExt ".obj"
#define kPefDebugExt  ".dbg"
#define kPefDriverExt ".sys"

#define kPefZero128 ".zero128"
#define kPefCode128 ".code128"
#define kPefData128 ".data128"

#define kPefZero64 ".zero64"
#define kPefCode64 ".code64"
#define kPefData64 ".data64"

#define kPefMagicLen (5)

#define kPefVersion (3)
#define kPefNameLen (255)

#define kPefBaseOrigin (0x40000000)

#define kPefStart "__ImageStart"

namespace LibCompiler
{
	enum
	{
		kPefArchStart	 = 99,
		kPefArchIntel86S = 100,
		kPefArchAMD64,
		kPefArchRISCV,
		kPefArch64000, /* 64x0 RISC architecture. */
		kPefArch32000,
		kPefArchPowerPC, /* 64-bit POWER architecture. */
		kPefArchARM64,
		kPefArchCount	= (kPefArchPowerPC - kPefArchIntel86S) + 1,
		kPefArchInvalid = 0xFF,
	};

	enum
	{
		kPefSubArchAMD,
		kPefSubArchIntel,
		kPefSubArchARM,
		kPefSubArchGeneric,
		kPefSubArchIBM,
	};

	enum
	{
		kPefKindExec   = 1, /* .o */
		kPefKindDylib  = 2, /* .dylib */
		kPefKindObject = 4, /* .obj */
		kPefKindDebug  = 5, /* .dbg */
		kPefKindDriver = 6,
		kPefKindCount,
	};

	/* PEF container */
	typedef struct PEFContainer final
	{
		CharType Magic[kPefMagicLen];
		UInt32	 Linker;
		UInt32	 Version;
		UInt32	 Kind;
		UInt32	 Abi;
		UInt32	 Cpu;
		UInt32	 SubCpu; /* Cpu specific information */
		UIntPtr	 Start;	 /* Origin of code */
		SizeType HdrSz;	 /* Size of header */
		SizeType Count;	 /* container header count */
	} PACKED PEFContainer;

	/* First PEFCommandHeader starts after PEFContainer */
	/* Last container is __exec_end */

	/* PEF executable section and commands. */

	typedef struct PEFCommandHeader final
	{
		CharType Name[kPefNameLen]; /* container name */
		UInt32	 Cpu;				/* container cpu */
		UInt32	 SubCpu;			/* container sub-cpu */
		UInt32	 Flags;				/* container flags */
		UInt16	 Kind;				/* container kind */
		UIntPtr	 Offset;			/* file offset */
		SizeType Size;				/* file size */
	} PACKED PEFCommandHeader;

	enum
	{
		kPefCode	 = 0xC,
		kPefData	 = 0xD,
		kPefZero	 = 0xE,
		kPefLinkerID = 0x1,
		kPefCount	 = 4,
		kPefInvalid	 = 0xFF,
	};
} // namespace LibCompiler

inline std::ofstream& operator<<(std::ofstream&				fp,
								 LibCompiler::PEFContainer& container)
{
	fp.write((char*)&container, sizeof(LibCompiler::PEFContainer));
	return fp;
}

inline std::ofstream& operator<<(std::ofstream&					fp,
								 LibCompiler::PEFCommandHeader& container)
{
	fp.write((char*)&container, sizeof(LibCompiler::PEFCommandHeader));
	return fp;
}

inline std::ifstream& operator>>(std::ifstream&				fp,
								 LibCompiler::PEFContainer& container)
{
	fp.read((char*)&container, sizeof(LibCompiler::PEFContainer));
	return fp;
}

inline std::ifstream& operator>>(std::ifstream&					fp,
								 LibCompiler::PEFCommandHeader& container)
{
	fp.read((char*)&container, sizeof(LibCompiler::PEFCommandHeader));
	return fp;
}
