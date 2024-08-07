/* -------------------------------------------

	Copyright ZKA Technologies

------------------------------------------- */

#pragma once

#include <NDKKit/AsmKit/AsmKit.hxx>
#include <NDKKit/Parser.hxx>

using namespace NDK;

namespace detail
{
	extern void print_error_asm(std::string reason, std::string file) noexcept;
	extern void print_warning_asm(std::string reason, std::string file) noexcept;
} // namespace detail

/// @brief Get Number from lineBuffer.
/// @param lineBuffer the lineBuffer to fetch from.
/// @param numberKey where to seek that number.
/// @return
static NumberCast32 GetNumber32(std::string lineBuffer, std::string numberKey)
{
	auto pos = lineBuffer.find(numberKey) + numberKey.size();

	if (lineBuffer.find(",") != std::string::npos)
		lineBuffer.erase(lineBuffer.find(","), 1);

	while (lineBuffer[pos] == ' ')
		++pos;

	switch (lineBuffer[pos + 1])
	{
	case 'x': {
		if (auto res = strtol(lineBuffer.substr(pos).c_str(), nullptr, 16); !res)
		{
			if (errno != 0)
			{
				detail::print_error_asm("invalid hex number: " + lineBuffer, "asm");
				throw std::runtime_error("invalid_hex");
			}
		}

		NumberCast32 numOffset(strtol(lineBuffer.substr(pos).c_str(), nullptr, 16));

		if (kVerbose)
		{
			kStdOut << "asm: found a base 16 number here: " << lineBuffer.substr(pos)
					<< "\n";
		}

		return numOffset;
	}
	case 'b': {
		if (auto res = strtol(lineBuffer.substr(pos).c_str(), nullptr, 2); !res)
		{
			if (errno != 0)
			{
				detail::print_error_asm("invalid binary number:" + lineBuffer, "asm");
				throw std::runtime_error("invalid_bin");
			}
		}

		NumberCast32 numOffset(strtol(lineBuffer.substr(pos).c_str(), nullptr, 2));

		if (kVerbose)
		{
			kStdOut << "asm: found a base 2 number here:" << lineBuffer.substr(pos)
					<< "\n";
		}

		return numOffset;
	}
	case 'o': {
		if (auto res = strtol(lineBuffer.substr(pos).c_str(), nullptr, 7); !res)
		{
			if (errno != 0)
			{
				detail::print_error_asm("invalid octal number: " + lineBuffer, "asm");
				throw std::runtime_error("invalid_octal");
			}
		}

		NumberCast32 numOffset(strtol(lineBuffer.substr(pos).c_str(), nullptr, 7));

		if (kVerbose)
		{
			kStdOut << "asm: found a base 8 number here:" << lineBuffer.substr(pos)
					<< "\n";
		}

		return numOffset;
	}
	default: {
		if (auto res = strtol(lineBuffer.substr(pos).c_str(), nullptr, 10); !res)
		{
			if (errno != 0)
			{
				detail::print_error_asm("invalid hex number: " + lineBuffer, "asm");
				throw std::runtime_error("invalid_hex");
			}
		}

		NumberCast32 numOffset(strtol(lineBuffer.substr(pos).c_str(), nullptr, 10));

		if (kVerbose)
		{
			kStdOut << "asm: found a base 10 number here:" << lineBuffer.substr(pos)
					<< "\n";
		}

		return numOffset;
	}
	}
}
