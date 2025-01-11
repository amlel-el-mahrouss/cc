/* -------------------------------------------

	Copyright (C) 2024 t& Corporation, all rights reserved

------------------------------------------- */

/// bugs: 0

/////////////////////////////////////////////////////////////////////////////////////////

// @file 32asm.cxx
// @author EL Mahrouss Amlal
// @brief 32x0 Assembler.

// REMINDER: when dealing with an undefined symbol use (string
// size):LinkerFindSymbol:(string) so that ld will look for it.

/////////////////////////////////////////////////////////////////////////////////////////

#define __ASM_NEED_32x0__ 1

#include <LibCompiler/AAL/CPU/32x0.h>
#include <LibCompiler/Parser.h>
#include <LibCompiler/NFC/AE.h>
#include <LibCompiler/NFC/PEF.h>

/////////////////////

// ANSI ESCAPE CODES

/////////////////////

#define kBlank	"\e[0;30m"
#define kRed	"\e[0;31m"
#define kWhite	"\e[0;97m"
#define kYellow "\e[0;33m"

#define kStdOut (std::cout << kWhite)
#define kStdErr (std::cout << kRed)

/////////////////////////////////////////////////////////////////////////////////////////

// @brief 32x0 Assembler entrypoint, the program/module starts here.

/////////////////////////////////////////////////////////////////////////////////////////

LIBCOMPILER_MODULE(ZKAAssemblerMain32000)
{
	return 0;
}
