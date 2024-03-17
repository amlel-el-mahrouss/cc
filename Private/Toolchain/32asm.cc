/* -------------------------------------------

    Copyright Mahrouss Logic

------------------------------------------- */

/// bugs: 0

/////////////////////////////////////////////////////////////////////////////////////////

// @file 32asm.cxx
// @author Amlal El Mahrouss
// @brief 32x0 Assembler.

// REMINDER: when dealing with an undefined symbol use (string
// size):LinkerFindSymbol:(string) so that ld will look for it.

/////////////////////////////////////////////////////////////////////////////////////////

#define __ASM_NEED_32x0__ 1

#include <CompilerKit/AsmKit/Arch/32x0.hpp>
#include <CompilerKit/ParserKit.hpp>
#include <CompilerKit/StdKit/AE.hpp>
#include <CompilerKit/StdKit/PEF.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>


/////////////////////

// ANSI ESCAPE CODES

/////////////////////

#define kBlank "\e[0;30m"
#define kRed "\e[0;31m"
#define kWhite "\e[0;97m"
#define kYellow "\e[0;33m"

#define kStdOut (std::cout << kWhite)
#define kStdErr (std::cout << kRed)

/////////////////////////////////////////////////////////////////////////////////////////

// @brief 32x0 Assembler entrypoint, the program/module starts here.

/////////////////////////////////////////////////////////////////////////////////////////

MPCC_MODULE(HCoreAssembler32000) {
  return 0;
}
