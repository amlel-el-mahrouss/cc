/*
 *	========================================================
 *
 *	c++-drv
 * 	Copyright (C) 2024 Theater Quality Inc, all rights reserved.
 *
 * 	========================================================
 */

/// BUGS: 1

#include <cstdio>
#define kPrintF printf

#define kExitOK	(EXIT_SUCCESS)
#define kExitNO (EXIT_FAILURE)

#define kSplashCxx() \
	kPrintF(kWhite "%s\n", "TQ Media C++ Compiler Driver, (c) 2024 Theater Quality Incorporated, all rights reserved.")

// extern_segment, @autodelete { ... }, fn foo() -> auto { ... }

#include <LibCompiler/AAL/CPU/amd64.h>
#include <LibCompiler/Parser.h>
#include <LibCompiler/UUID.h>

/* ZKA C++ Compiler */
/* This is part of the LibCompiler. */
/* (c) Theater Quality Incorporated */

/// @author EL Mahrouss Amlal (amlel)
/// @file CPlusPlusCompilerAMD64.cxx
/// @brief Optimized C++ Compiler Driver.
/// @todo Throw error for scoped inside scoped variables when they get referenced outside.
/// @todo Add class/struct/enum support.

///////////////////////

// ANSI ESCAPE CODES //

///////////////////////

#define kBlank "\e[0;30m"
#define kRed   "\e[0;31m"
#define kWhite "\e[0;97m"

/////////////////////////////////////

// INTERNALS OF THE C++ COMPILER

/////////////////////////////////////

/// @internal
namespace Details
{
	std::filesystem::path expand_home(const std::filesystem::path& p)
	{
		if (!p.empty() && p.string()[0] == '~')
		{
			const char* home = std::getenv("HOME"); // For Unix-like systems
			if (!home)
			{
				home = std::getenv("USERPROFILE"); // For Windows
			}
			if (home)
			{
				return std::filesystem::path(home) / p.relative_path().string().substr(1);
			}
			else
			{
				throw std::runtime_error("Home directory not found in environment variables");
			}
		}
		return p;
	}

	struct CompilerRegisterMap final
	{
		std::string fName;
		std::string fReg;
	};

	// \brief Offset based struct/class
	struct CompilerStructMap final
	{
		std::string fName;
		std::string fReg;

		// offset counter
		std::size_t fOffsetsCnt;

		// offset array
		std::vector<std::pair<Int32, std::string>> fOffsets;
	};

	struct CompilerState final
	{
		std::vector<CompilerRegisterMap> fStackMapVector;
		std::vector<CompilerStructMap>	 fStructMapVector;
		LibCompiler::SyntaxLeafList*	 fSyntaxTree{nullptr};
		std::unique_ptr<std::ofstream>	 fOutputAssembly;
		std::string						 fLastFile;
		std::string						 fLastError;
		bool							 fVerbose;
	};
} // namespace Details

static Details::CompilerState kState;
static SizeType				 kErrorLimit = 100;

static Int32 kAcceptableErrors = 0;

namespace Details
{
	/// @brief prints an error into stdout.
	/// @param reason the reason of the error.
	/// @param file where does it originate from?
	void print_error(std::string reason, std::string file) noexcept;

	struct CompilerType final
	{
		std::string fName;
		std::string fValue;
	};
} // namespace Details

/////////////////////////////////////////////////////////////////////////////////////////

// Target architecture.
static int kMachine = LibCompiler::AssemblyFactory::kArchAMD64;

/////////////////////////////////////////

// ARGUMENTS REGISTERS (R8, R15)

/////////////////////////////////////////

static size_t									  kRegisterCnt	   = kAsmRegisterLimit;
static size_t									  kStartUsable	   = 8;
static size_t									  kUsableLimit	   = 15;
static size_t									  kRegisterCounter = kStartUsable;
static std::vector<LibCompiler::CompilerKeyword> kKeywords;

/////////////////////////////////////////

// COMPILER PARSING UTILITIES/STATES.

/////////////////////////////////////////

static std::vector<std::string>		 kFileList;
static LibCompiler::AssemblyFactory kFactory;
static bool							 kInStruct	  = false;
static bool							 kOnWhileLoop = false;
static bool							 kOnForLoop	  = false;
static bool							 kInBraces	  = false;
static size_t						 kBracesCount = 0UL;

/* @brief C++ compiler backend for the ZKA C++ driver */
class CompilerFrontendCPlusPlus final : public LibCompiler::ICompilerFrontend
{
public:
	explicit CompilerFrontendCPlusPlus()  = default;
	~CompilerFrontendCPlusPlus() override = default;

	TOOLCHAINKIT_COPY_DEFAULT(CompilerFrontendCPlusPlus);

	bool Compile(const std::string text, const std::string file) override;

	const char* Language() override;
};

/// @internal compiler variables

static CompilerFrontendCPlusPlus* kCompilerFrontend = nullptr;

static std::vector<std::string> kRegisterMap;

static std::vector<std::string> kRegisterList = {
	"rbx",
	"rsi",
	"r10",
	"r11",
	"r12",
	"r13",
	"r14",
	"r15",
	"xmm12",
	"xmm13",
	"xmm14",
	"xmm15",
};

/// @brief The PEF calling convention (caller must save rax, rbp)
/// @note callee must return via **rax**.
static std::vector<std::string> kRegisterConventionCallList = {
	"r8",
	"r9",
	"r10",
	"r11",
	"r12",
	"r13",
	"r14",
	"r15",
};

static std::size_t kFunctionEmbedLevel = 0UL;

/// detail namespaces

const char* CompilerFrontendCPlusPlus::Language()
{
	return "ZKA C++";
}

/////////////////////////////////////////////////////////////////////////////////////////

/// @name Compile
/// @brief Generate assembly from a C++ source.

/////////////////////////////////////////////////////////////////////////////////////////

bool CompilerFrontendCPlusPlus::Compile(const std::string text,
										const std::string file)
{
	if (text.empty())
		return false;

	std::size_t														   index = 0UL;
	std::vector<std::pair<LibCompiler::CompilerKeyword, std::size_t>> keywords_list;

	bool		found		 = false;
	static bool commentBlock = false;

	for (auto& keyword : kKeywords)
	{
		if (text.find(keyword.keyword_name) != std::string::npos)
		{
			switch (keyword.keyword_kind)
			{
			case LibCompiler::eKeywordKindCommentMultiLineStart: {
				commentBlock = true;
				return true;
			}
			case LibCompiler::eKeywordKindCommentMultiLineEnd: {
				commentBlock = false;
				break;
			}
			case LibCompiler::eKeywordKindCommentInline: {
				break;
			}
			default:
				break;
			}

			if (text[text.find(keyword.keyword_name) - 1] == '+' &&
				keyword.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableAssign)
				continue;

			if (text[text.find(keyword.keyword_name) - 1] == '-' &&
				keyword.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableAssign)
				continue;

			if (text[text.find(keyword.keyword_name) + 1] == '=' &&
				keyword.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableAssign)
				continue;

			keywords_list.emplace_back(std::make_pair(keyword, index));
			++index;

			found = true;
		}
	}

	if (!found && !commentBlock)
	{
		for (size_t i = 0; i < text.size(); i++)
		{
			if (isalnum(text[i]))
			{
				Details::print_error("syntax error: " + text, file);
				return false;
			}
		}
	}

	for (auto& keyword : keywords_list)
	{
		auto syntax_tree = LibCompiler::SyntaxLeafList::SyntaxLeaf();

		switch (keyword.first.keyword_kind)
		{
		case LibCompiler::KeywordKind::eKeywordKindIf: {
			auto expr = text.substr(text.find(keyword.first.keyword_name) + keyword.first.keyword_name.size() + 1, text.find(")") - 1);

			if (expr.find(">=") != std::string::npos)
			{
				auto left  = text.substr(text.find(keyword.first.keyword_name) + keyword.first.keyword_name.size() + 2, expr.find("<=") + strlen("<="));
				auto right = text.substr(expr.find(">=") + strlen(">="), text.find(")") - 1);

				size_t i = right.size() - 1;

				try
				{
					while (!std::isalnum(right[i]))
					{
						right.erase(i, 1);
						--i;
					}

					right.erase(0, i);
				}
				catch (...)
				{
					right.erase(0, i);
				}

				i = left.size() - 1;
				try
				{
					while (!std::isalnum(left[i]))
					{
						left.erase(i, 1);
						--i;
					}

					left.erase(0, i);
				}
				catch (...)
				{
					left.erase(0, i);
				}

				if (!isdigit(left[0]) ||
					!isdigit(right[0]))
				{
					auto indexRight = 0UL;

					auto& valueOfVar = !isdigit(left[0]) ? left : right;

					for (auto pairRight : kRegisterMap)
					{
						++indexRight;

						if (pairRight != valueOfVar)
						{

							auto& valueOfVarOpposite = isdigit(left[0]) ? left : right;

							syntax_tree.fUserValue += "mov " + kRegisterList[indexRight + 1] + ", " + valueOfVarOpposite + "\n";
							syntax_tree.fUserValue += "cmp " + kRegisterList[kRegisterMap.size() - 1] + "," + kRegisterList[indexRight + 1] + "\n";

							goto done_iterarting_on_if;
						}

						auto& valueOfVarOpposite = isdigit(left[0]) ? left : right;

						syntax_tree.fUserValue += "mov " + kRegisterList[indexRight + 1] + ", " + valueOfVarOpposite + "\n";
						syntax_tree.fUserValue += "cmp " + kRegisterList[kRegisterMap.size() - 1] + ", " + kRegisterList[indexRight + 1] + "\n";

						break;
					}
				}

			done_iterarting_on_if:

				std::string fnName = text;
				fnName.erase(fnName.find(keyword.first.keyword_name));

				for (auto& ch : fnName)
				{
					if (ch == ' ')
						ch = '_';
				}

				syntax_tree.fUserValue += "jge __OFFSET_ON_TRUE_NDK\nsegment .code64 __OFFSET_ON_TRUE_NDK:\n";
			}

			break;
		}
		case LibCompiler::KeywordKind::eKeywordKindFunctionStart: {
			for (auto& ch : text)
			{
				if (isdigit(ch))
				{
					goto dont_accept;
				}
			}

			goto accept;

		dont_accept:
			return false;

		accept:
			std::string fnName = text;
			size_t indexFnName = 0;

			// this one is for the type.
			for (auto& ch : text)
			{
				++indexFnName;

				if (ch == '\t')
					break;

				if (ch == ' ')
					break;
			}

			fnName = text.substr(indexFnName);

			if (text.ends_with(";"))
				goto tk_write_assembly;
			else if (text.size() <= indexFnName)
				Details::print_error("Invalid function name: " + fnName, file);

			indexFnName = 0;

			for (auto& ch : fnName)
			{
				if (ch == ' ' ||
					ch == '\t')
				{
					if (fnName[indexFnName - 1] != ')')
						Details::print_error("Invalid function name: " + fnName, file);
				
					if ((indexFnName + 1) != fnName.size())
						Details::print_error("Extra characters after function name: " + fnName, file);
				}

				++indexFnName;
			}

			syntax_tree.fUserValue = "public_segment .code64 __TOOLCHAINKIT_" + fnName + "\n";
			++kFunctionEmbedLevel;

			break;

tk_write_assembly:
			syntax_tree.fUserValue = "jmp __TOOLCHAINKIT_" + fnName + "\n";
		}
		case LibCompiler::KeywordKind::eKeywordKindFunctionEnd: {
			if (text.ends_with(";"))
				break;

			--kFunctionEmbedLevel;

			if (kRegisterMap.size() > kRegisterList.size())
			{
				--kFunctionEmbedLevel;
			}

			if (kFunctionEmbedLevel < 1)
				kRegisterMap.clear();
			break;
		}
		case LibCompiler::KeywordKind::eKeywordKindEndInstr:
		case LibCompiler::KeywordKind::eKeywordKindVariableInc:
		case LibCompiler::KeywordKind::eKeywordKindVariableDec:
		case LibCompiler::KeywordKind::eKeywordKindVariableAssign: {
			std::string valueOfVar = "";

			if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableInc)
			{
				valueOfVar = text.substr(text.find("+=") + 2);
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableDec)
			{
				valueOfVar = text.substr(text.find("-=") + 2);
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableAssign)
			{
				valueOfVar = text.substr(text.find("=") + 1);
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindEndInstr)
			{
				break;
			}

			while (valueOfVar.find(";") != std::string::npos &&
				   keyword.first.keyword_kind != LibCompiler::KeywordKind::eKeywordKindEndInstr)
			{
				valueOfVar.erase(valueOfVar.find(";"));
			}

			std::string varName = text;

			if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableInc)
			{
				varName.erase(varName.find("+="));
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableDec)
			{
				varName.erase(varName.find("-="));
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableAssign)
			{
				varName.erase(varName.find("="));
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindEndInstr)
			{
				varName.erase(varName.find(";"));
			}

			static bool typeFound = false;

			for (auto& keyword : kKeywords)
			{
				if (keyword.keyword_kind == LibCompiler::eKeywordKindType)
				{
					if (text.find(keyword.keyword_name) != std::string::npos)
					{
						if (text[text.find(keyword.keyword_name)] == ' ')
						{
							typeFound = false;
							continue;
						}

						typeFound = true;
					}
				}
			}

			std::string instr = "mov ";

			if (typeFound && keyword.first.keyword_kind != LibCompiler::KeywordKind::eKeywordKindVariableInc &&
				keyword.first.keyword_kind != LibCompiler::KeywordKind::eKeywordKindVariableDec)
			{
				if (kRegisterMap.size() > kRegisterList.size())
				{
					++kFunctionEmbedLevel;
				}

				while (varName.find(" ") != std::string::npos)
				{
					varName.erase(varName.find(" "), 1);
				}

				while (varName.find("\t") != std::string::npos)
				{
					varName.erase(varName.find("\t"), 1);
				}

				for (size_t i = 0; !isalnum(valueOfVar[i]); i++)
				{
					if (i > valueOfVar.size())
						break;

					valueOfVar.erase(i, 1);
				}

				constexpr auto cTrueVal	 = "true";
				constexpr auto cFalseVal = "false";

				if (valueOfVar == cTrueVal)
				{
					valueOfVar = "1";
				}
				else if (valueOfVar == cFalseVal)
				{
					valueOfVar = "0";
				}

				std::size_t indexRight = 0UL;

				for (auto pairRight : kRegisterMap)
				{
					++indexRight;

					if (pairRight != valueOfVar)
					{
						if (valueOfVar[0] == '\"')
						{

							syntax_tree.fUserValue = "segment .data64 __TOOLCHAINKIT_LOCAL_VAR_" + varName + ": db " + valueOfVar + ", 0\n\n";
							syntax_tree.fUserValue += instr + kRegisterList[kRegisterMap.size() - 1] + ", " + "__TOOLCHAINKIT_LOCAL_VAR_" + varName + "\n";
						}
						else
						{
							syntax_tree.fUserValue = instr + kRegisterList[kRegisterMap.size() - 1] + ", " + valueOfVar + "\n";
						}

						goto done;
					}
				}

				if (((int)indexRight - 1) < 0)
				{
					if (valueOfVar[0] == '\"')
					{

						syntax_tree.fUserValue = "segment .data64 __TOOLCHAINKIT_LOCAL_VAR_" + varName + ": db " + valueOfVar + ", 0\n";
						syntax_tree.fUserValue += instr + kRegisterList[kRegisterMap.size()] + ", " + "__TOOLCHAINKIT_LOCAL_VAR_" + varName + "\n";
					}
					else
					{
						syntax_tree.fUserValue = instr + kRegisterList[kRegisterMap.size()] + ", " + valueOfVar + "\n";
					}

					goto done;
				}

				if (valueOfVar[0] != '\"' &&
					valueOfVar[0] != '\'' &&
					!isdigit(valueOfVar[0]))
				{
					for (auto pair : kRegisterMap)
					{
						if (pair == valueOfVar)
							goto done;
					}

					Details::print_error("Variable not declared: " + varName, file);
					return false;
				}

			done:
				for (auto& keyword : kKeywords)
				{
					if (keyword.keyword_kind == LibCompiler::eKeywordKindType &&
						varName.find(keyword.keyword_name) != std::string::npos)
					{
						varName.erase(varName.find(keyword.keyword_name), keyword.keyword_name.size());
						break;
					}
				}

				kRegisterMap.push_back(varName);

				break;
			}

			if (kKeywords[keyword.second - 1].keyword_kind == LibCompiler::eKeywordKindType ||
				kKeywords[keyword.second - 1].keyword_kind == LibCompiler::eKeywordKindTypePtr)
			{
				syntax_tree.fUserValue = "\n";
				continue;
			}

			if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindEndInstr)
			{
				syntax_tree.fUserValue = "\n";
				continue;
			}

			if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableInc)
			{
				instr = "add ";
			}
			else if (keyword.first.keyword_kind == LibCompiler::KeywordKind::eKeywordKindVariableDec)
			{
				instr = "sub ";
			}

			std::string varErrCpy = varName;

			while (varName.find(" ") != std::string::npos)
			{
				varName.erase(varName.find(" "), 1);
			}

			while (varName.find("\t") != std::string::npos)
			{
				varName.erase(varName.find("\t"), 1);
			}

			std::size_t indxReg = 0UL;

			for (size_t i = 0; !isalnum(valueOfVar[i]); i++)
			{
				if (i > valueOfVar.size())
					break;

				valueOfVar.erase(i, 1);
			}

			while (valueOfVar.find(" ") != std::string::npos)
			{
				valueOfVar.erase(valueOfVar.find(" "), 1);
			}

			while (valueOfVar.find("\t") != std::string::npos)
			{
				valueOfVar.erase(valueOfVar.find("\t"), 1);
			}

			constexpr auto cTrueVal	 = "true";
			constexpr auto cFalseVal = "false";

			/// interpet boolean values, since we're on C++

			if (valueOfVar == cTrueVal)
			{
				valueOfVar = "1";
			}
			else if (valueOfVar == cFalseVal)
			{
				valueOfVar = "0";
			}

			for (auto pair : kRegisterMap)
			{
				++indxReg;

				if (pair != varName)
					continue;

				std::size_t indexRight = 0ul;

				for (auto pairRight : kRegisterMap)
				{
					++indexRight;

					if (pairRight != varName)
					{
						syntax_tree.fUserValue = instr + kRegisterList[kRegisterMap.size()] + ", " + valueOfVar + "\n";
						continue;
					}

					syntax_tree.fUserValue = instr + kRegisterList[indexRight - 1] + ", " + valueOfVar + "\n";
					break;
				}

				break;
			}

			if (syntax_tree.fUserValue.empty())
			{
				Details::print_error("Variable not declared: " + varErrCpy, file);
			}

			break;
		}
		case LibCompiler::KeywordKind::eKeywordKindReturn: {
			try
			{
				auto		pos		= text.find("return") + strlen("return") + 1;
				std::string subText = text.substr(pos);
				subText				= subText.erase(subText.find(";"));
				size_t indxReg		= 0UL;

				if (subText[0] != '\"' &&
					subText[0] != '\'')
				{
					if (!isdigit(subText[0]))
					{
						for (auto pair : kRegisterMap)
						{
							++indxReg;

							if (pair != subText)
								continue;

							syntax_tree.fUserValue = "mov rax," + kRegisterList[indxReg - 1] + "\r\nret\n";
							break;
						}

						if (syntax_tree.fUserValue.empty())
						{
							Details::print_error("Variable not declared: " + subText, file);
						}
					}
					else
					{
						syntax_tree.fUserValue = "mov rax, " + subText + "\r\nret\n";
					}
				}
				else
				{
					syntax_tree.fUserValue = "__TOOLCHAINKIT_LOCAL_RETURN_STRING: db " + subText + ", 0\nmov rcx, __TOOLCHAINKIT_LOCAL_RETURN_STRING\n";
					syntax_tree.fUserValue += "mov rax, rcx\r\nret\n";
				}

				break;
			}
			catch (...)
			{
				syntax_tree.fUserValue = "ret\n";
			}
		}
		default:
		{
			break;
		}
		}

		syntax_tree.fUserData = keyword.first;
		kState.fSyntaxTree->fLeafList.push_back(syntax_tree);
	}

ndk_compile_ok:
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief C++ assembler class.
 */

/////////////////////////////////////////////////////////////////////////////////////////

class AssemblyCPlusPlusInterface final ASSEMBLY_INTERFACE
{
public:
	explicit AssemblyCPlusPlusInterface()  = default;
	~AssemblyCPlusPlusInterface() override = default;

	TOOLCHAINKIT_COPY_DEFAULT(AssemblyCPlusPlusInterface);

	[[maybe_unused]] static Int32 Arch() noexcept
	{
		return LibCompiler::AssemblyFactory::kArchAMD64;
	}

	Int32 CompileToFormat(std::string& src, Int32 arch) override
	{
		if (arch != AssemblyCPlusPlusInterface::Arch())
			return 1;

		if (kCompilerFrontend == nullptr)
			return 1;

		/* @brief copy contents wihtout extension */
		std::string	  src_file = src;
		std::ifstream src_fp   = std::ifstream(src_file, std::ios::in);

		const char* cExts[] = kAsmFileExts;

		std::string dest = src_file;
		dest += cExts[2];

		if (dest.empty())
		{
			dest = "CXX-LibCompiler-";

			std::random_device rd;
			auto			   seed_data = std::array<int, std::mt19937::state_size>{};

			std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));

			std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
			std::mt19937  generator(seq);

			auto gen = uuids::uuid_random_generator(generator);

			auto id = gen();
			dest += uuids::to_string(id);
		}

		kState.fOutputAssembly = std::make_unique<std::ofstream>(dest);

		auto fmt = LibCompiler::current_date();

		(*kState.fOutputAssembly) << "; Repository Path: /" << src_file << "\n";

		std::filesystem::path path = std::filesystem::path("./");

		while (path != Details::expand_home(std::filesystem::path("~")))
		{
			for (auto const& dir_entry : std::filesystem::recursive_directory_iterator{path})
			{
				if (dir_entry.is_directory() &&
					dir_entry.path().string().find(".git") != std::string::npos)
					goto break_loop;
			}

			path = path.parent_path();
		break_loop:
			(*kState.fOutputAssembly) << "; Repository Style: Git\n";
			break;
		}

		(*kState.fOutputAssembly)
			<< "; Assembler Dialect: AMD64 LibCompiler Assembler. (Generated from C++)\n";
		(*kState.fOutputAssembly) << "; Date: " << fmt << "\n";
		(*kState.fOutputAssembly) << "#bits 64\n#org 0x1000000"
								  << "\n";

		kState.fSyntaxTree = new LibCompiler::SyntaxLeafList();

		// ===================================
		// Parse source file.
		// ===================================

		std::string line_source;

		while (std::getline(src_fp, line_source))
		{
			kCompilerFrontend->Compile(line_source, src);
		}

		for (auto& ast_generated : kState.fSyntaxTree->fLeafList)
		{
			(*kState.fOutputAssembly) << ast_generated.fUserValue;
		}

		kState.fOutputAssembly->flush();
		kState.fOutputAssembly->close();

		delete kState.fSyntaxTree;
		kState.fSyntaxTree = nullptr;

		if (kAcceptableErrors > 0)
			return 1;

		return kExitOK;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////

static void cxx_print_help()
{
	kSplashCxx();
	kPrintF("%s", "No help available, see:\n");
	kPrintF("%s", "www.zws.zka.com/help/c++lang\n");
}

/////////////////////////////////////////////////////////////////////////////////////////

#define kExtListCxx                          \
	{                                        \
		".cpp", ".cxx", ".cc", ".c++", ".cp" \
	}

TOOLCHAINKIT_MODULE(CompilerCPlusPlusX8664)
{
	bool skip = false;

	kKeywords.push_back({.keyword_name = "if", .keyword_kind = LibCompiler::eKeywordKindIf});
	kKeywords.push_back({.keyword_name = "else", .keyword_kind = LibCompiler::eKeywordKindElse});
	kKeywords.push_back({.keyword_name = "else if", .keyword_kind = LibCompiler::eKeywordKindElseIf});

	kKeywords.push_back({.keyword_name = "class", .keyword_kind = LibCompiler::eKeywordKindClass});
	kKeywords.push_back({.keyword_name = "struct", .keyword_kind = LibCompiler::eKeywordKindClass});
	kKeywords.push_back({.keyword_name = "namespace", .keyword_kind = LibCompiler::eKeywordKindNamespace});
	kKeywords.push_back({.keyword_name = "typedef", .keyword_kind = LibCompiler::eKeywordKindTypedef});
	kKeywords.push_back({.keyword_name = "using", .keyword_kind = LibCompiler::eKeywordKindTypedef});
	kKeywords.push_back({.keyword_name = "{", .keyword_kind = LibCompiler::eKeywordKindBodyStart});
	kKeywords.push_back({.keyword_name = "}", .keyword_kind = LibCompiler::eKeywordKindBodyEnd});
	kKeywords.push_back({.keyword_name = "auto", .keyword_kind = LibCompiler::eKeywordKindVariable});
	kKeywords.push_back({.keyword_name = "int", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "bool", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "unsigned", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "short", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "char", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "long", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "float", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "double", .keyword_kind = LibCompiler::eKeywordKindType});
	kKeywords.push_back({.keyword_name = "void", .keyword_kind = LibCompiler::eKeywordKindType});

	kKeywords.push_back({.keyword_name = "auto*", .keyword_kind = LibCompiler::eKeywordKindVariablePtr});
	kKeywords.push_back({.keyword_name = "int*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "bool*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "unsigned*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "short*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "char*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "long*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "float*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "double*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});
	kKeywords.push_back({.keyword_name = "void*", .keyword_kind = LibCompiler::eKeywordKindTypePtr});

	kKeywords.push_back({.keyword_name = "(", .keyword_kind = LibCompiler::eKeywordKindFunctionStart});
	kKeywords.push_back({.keyword_name = ")", .keyword_kind = LibCompiler::eKeywordKindFunctionEnd});
	kKeywords.push_back({.keyword_name = "=", .keyword_kind = LibCompiler::eKeywordKindVariableAssign});
	kKeywords.push_back({.keyword_name = "+=", .keyword_kind = LibCompiler::eKeywordKindVariableInc});
	kKeywords.push_back({.keyword_name = "-=", .keyword_kind = LibCompiler::eKeywordKindVariableDec});
	kKeywords.push_back({.keyword_name = "const", .keyword_kind = LibCompiler::eKeywordKindConstant});
	kKeywords.push_back({.keyword_name = "*", .keyword_kind = LibCompiler::eKeywordKindPtr});
	kKeywords.push_back({.keyword_name = "->", .keyword_kind = LibCompiler::eKeywordKindPtrAccess});
	kKeywords.push_back({.keyword_name = ".", .keyword_kind = LibCompiler::eKeywordKindAccess});
	kKeywords.push_back({.keyword_name = ",", .keyword_kind = LibCompiler::eKeywordKindArgSeparator});
	kKeywords.push_back({.keyword_name = ";", .keyword_kind = LibCompiler::eKeywordKindEndInstr});
	kKeywords.push_back({.keyword_name = ":", .keyword_kind = LibCompiler::eKeywordKindSpecifier});
	kKeywords.push_back({.keyword_name = "public:", .keyword_kind = LibCompiler::eKeywordKindSpecifier});
	kKeywords.push_back({.keyword_name = "private:", .keyword_kind = LibCompiler::eKeywordKindSpecifier});
	kKeywords.push_back({.keyword_name = "protected:", .keyword_kind = LibCompiler::eKeywordKindSpecifier});
	kKeywords.push_back({.keyword_name = "final", .keyword_kind = LibCompiler::eKeywordKindSpecifier});
	kKeywords.push_back({.keyword_name = "return", .keyword_kind = LibCompiler::eKeywordKindReturn});
	kKeywords.push_back({.keyword_name = "--*", .keyword_kind = LibCompiler::eKeywordKindCommentMultiLineStart});
	kKeywords.push_back({.keyword_name = "*/", .keyword_kind = LibCompiler::eKeywordKindCommentMultiLineStart});
	kKeywords.push_back({.keyword_name = "--/", .keyword_kind = LibCompiler::eKeywordKindCommentInline});
	kKeywords.push_back({.keyword_name = "==", .keyword_kind = LibCompiler::eKeywordKindEq});
	kKeywords.push_back({.keyword_name = "!=", .keyword_kind = LibCompiler::eKeywordKindNotEq});
	kKeywords.push_back({.keyword_name = ">=", .keyword_kind = LibCompiler::eKeywordKindGreaterEq});
	kKeywords.push_back({.keyword_name = "<=", .keyword_kind = LibCompiler::eKeywordKindLessEq});

	kFactory.Mount(new AssemblyCPlusPlusInterface());
	kCompilerFrontend = new CompilerFrontendCPlusPlus();

	for (auto index = 1UL; index < argc; ++index)
	{
		if (argv[index][0] == '-')
		{
			if (skip)
			{
				skip = false;
				continue;
			}

			if (strcmp(argv[index], "--cl:version") == 0)
			{
				kSplashCxx();
				return kExitOK;
			}

			if (strcmp(argv[index], "--cl:verbose") == 0)
			{
				kState.fVerbose = true;

				continue;
			}

			if (strcmp(argv[index], "--cl:h") == 0)
			{
				cxx_print_help();

				return kExitOK;
			}

			if (strcmp(argv[index], "--cl:c++-dialect") == 0)
			{
				if (kCompilerFrontend)
					std::cout << kCompilerFrontend->Language() << "\n";

				return kExitOK;
			}

			if (strcmp(argv[index], "--cl:max-err") == 0)
			{
				try
				{
					kErrorLimit = std::strtol(argv[index + 1], nullptr, 10);
				}
				// catch anything here
				catch (...)
				{
					kErrorLimit = 0;
				}

				skip = true;

				continue;
			}

			std::string err = "Unknown option: ";
			err += argv[index];

			Details::print_error(err, "c++-drv");

			continue;
		}

		kFileList.emplace_back(argv[index]);

		std::string argv_i = argv[index];

		std::vector exts  = kExtListCxx;
		bool		found = false;

		for (std::string ext : exts)
		{
			if (argv_i.find(ext) != std::string::npos)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			if (kState.fVerbose)
			{
				Details::print_error(argv_i + " is not a valid C++ source.\n", "c++-drv");
			}

			return 1;
		}

		std::cout << "CPlusPlusCompilerAMD64: Building: " << argv[index] << std::endl;

		if (kFactory.Compile(argv_i, kMachine) != kExitOK)
			return 1;
	}

	return kExitOK;
}

// Last rev 8-1-24
//
