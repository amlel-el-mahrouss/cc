/*
 *	========================================================
 *
 *	ccplus
 * 	Copyright WestCo, all rights reserved.
 *
 * 	========================================================
 */

#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <stack>
#include <utility>
#include <C++Kit/AsmKit/Arch/NewCPU.hpp>
#include <C++Kit/ParserKit.hpp>

#define kOk 0

/* WestCo C driver */
/* This is part of MP-UX C SDK. */
/* (c) WestCo */

/////////////////////

// ANSI ESCAPE CODES

/////////////////////

#define kBlank "\e[0;30m"
#define kRed "\e[0;31m"
#define kWhite "\e[0;97m"

/////////////////////////////////////

// INTERNAL STUFF OF THE C COMPILER

/////////////////////////////////////

namespace detail
{
    struct CompilerRegisterMap
    {
        std::string fName;
        std::string fRegister;
    };

    struct CompilerState
    {
        std::vector<ParserKit::SyntaxLeafList> fSyntaxTreeList;
        std::vector<CompilerRegisterMap> kStackFrame;
        ParserKit::SyntaxLeafList* fSyntaxTree{ nullptr };
        std::unique_ptr<std::ofstream> fOutputAssembly;
        std::string fLastFile;
        std::string fLastError;
        bool kVerbose;
    };
}

static detail::CompilerState kState;
static SizeType kErrorLimit = 100;

static Int32 kAcceptableErrors = 0;

namespace detail
{
    void print_error(std::string reason, std::string file) noexcept
    {
        if (reason[0] == '\n')
            reason.erase(0, 1);

        if (file.find(".pp") != std::string::npos)
        {
            file.erase(file.find(".pp"), 3);
        }

        if (kState.fLastFile != file)
        {
            std::cout << kRed << "[ ccplus ] " << kWhite << ((file == "ccplus") ? "internal compiler error " : ("in file, " + file)) << kBlank << std::endl;
            std::cout << kRed << "[ ccplus ] " << kWhite << reason << kBlank << std::endl;

            kState.fLastFile = file;
        }
        else
        {
            std::cout << kRed << "[ ccplus ] [ " << kState.fLastFile <<  " ] " << kWhite << reason << kBlank << std::endl;
        }

        if (kAcceptableErrors > kErrorLimit)
            std::exit(3);

        ++kAcceptableErrors;
    }

    struct CompilerType
    {
        std::string fName;
        std::string fValue;
    };
}

/////////////////////////////////////////////////////////////////////////////////////////

// Target architecture.
static int kMachine = 0;

/////////////////////////////////////////

// REGISTERS ACCORDING TO USED ASSEMBLER

/////////////////////////////////////////

static size_t kRegisterCnt = kAsmRegisterLimit;
static size_t kStartUsable = 1;
static size_t kUsableLimit = 14;
static size_t kRegisterCounter = kStartUsable;
static std::string kRegisterPrefix = kAsmRegisterPrefix;
static std::vector<std::string> kKeywords;

/////////////////////////////////////////

// COMPILER PARSING UTILITIES/STATES.

/////////////////////////////////////////

static std::vector<std::string> kFileList;
static CxxKit::AssemblyFactory kFactory;
static bool kInStruct = false;
static bool kOnWhileLoop = false;
static bool kOnForLoop = false;
static bool kInBraces = false;
static size_t kBracesCount = 0UL;

/* @brief C compiler backend for WestCo C */
class CompilerBackendClang final : public ParserKit::CompilerBackend
{
public:
    explicit CompilerBackendClang() = default;
    ~CompilerBackendClang() override = default;

    CXXKIT_COPY_DEFAULT(CompilerBackendClang);

    bool Compile(const std::string& text, const char* file) override;

    const char* Language() override { return "Optimized 64x0 C"; }

};

static CompilerBackendClang* kCompilerBackend = nullptr;
static std::vector<detail::CompilerType> kCompilerVariables;
static std::vector<std::string>          kCompilerFunctions;

// @brief this hook code before the begin/end command.
static std::string kAddIfAnyBegin;
static std::string kAddIfAnyEnd;
static std::string kLatestVar;

static std::string cxx_parse_function_call(std::string& _text)
{
    if (_text[0] == '(') {
        std::string substr;
        std::string args_buffer;
        std::string args;

        bool type_crossed = false;

        for (char substr_first_index: _text)
        {
            args_buffer += substr_first_index;

            if (substr_first_index == ';')
            {
                args_buffer = args_buffer.erase(0, args_buffer.find('('));
                args_buffer = args_buffer.erase(args_buffer.find(';'), 1);
                args_buffer = args_buffer.erase(args_buffer.find(')'), 1);
                args_buffer = args_buffer.erase(args_buffer.find('('), 1);

                if (!args_buffer.empty())
                    args += "\tpsh ";

                while (args_buffer.find(',') != std::string::npos)
                {
                    args_buffer.replace(args_buffer.find(','), 1, "\n\tpsh ");
                }

                args += args_buffer;
                args += "\n\tjlr __import ";
            }
        }

        return args;
    }

    return "";
}

#include <uuid/uuid.h>

namespace detail
{
    union number_type
    {
        number_type(UInt64 raw)
                : raw(raw)
        {}

        char number[8];
        UInt64 raw;
    };
}

/////////////////////////////////////////////////////////////////////////////////////////

// @name Compile
// @brief Generate MASM from a C source.

/////////////////////////////////////////////////////////////////////////////////////////

bool CompilerBackendClang::Compile(const std::string& text, const char* file)
{   
    if (text.empty())
        return false;

    // if (expr)
    // int name = expr;
    // expr;

    std::size_t index = 0UL;

    auto syntax_tree = ParserKit::SyntaxLeafList::SyntaxLeaf();

    syntax_tree.fUserData = text;
    kState.fSyntaxTree->fLeafList.push_back(syntax_tree);

    std::string text_cpy = text;

    std::vector<std::pair<std::string, std::size_t>> keywords_list;

    for (auto& keyword : kKeywords)
    {
        while (text_cpy.find(keyword) != std::string::npos)
        {
            keywords_list.push_back(std::make_pair(keyword, index));
            ++index;

            text_cpy.erase(text_cpy.find(keyword), keyword.size());
        }
    }

    // TODO: sort keywords

    for (auto& keyword : keywords_list)
    {
        syntax_tree.fUserData = keyword.first;
        kState.fSyntaxTree->fLeafList.push_back(syntax_tree);

        std::cout << keyword.first << "\n";
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief C To Assembly mount-point.
 */

/////////////////////////////////////////////////////////////////////////////////////////

class AssemblyMountpointClang final : public CxxKit::AssemblyMountpoint
{
public:
    explicit AssemblyMountpointClang() = default;
    ~AssemblyMountpointClang() override = default;

    CXXKIT_COPY_DEFAULT(AssemblyMountpointClang);

    [[maybe_unused]] static Int32 Arch() noexcept { return CxxKit::AssemblyFactory::kArchRISCV; }

    Int32 CompileToFormat(CxxKit::StringView& src, Int32 arch) override
    {
        if (arch != AssemblyMountpointClang::Arch())
            return -1;

        if (kCompilerBackend == nullptr)
            return -1;

        /* @brief copy contents wihtout extension */
        std::string src_file = src.CData();
        std::ifstream src_fp = std::ifstream(src_file, std::ios::in);
        std::string dest;

        for (auto& ch : src_file)
        {
            if (ch == '.')
            {
                break;
            }

            dest += ch;
        }

        /* According to pef abi. */
        dest += kAsmFileExt;

        kState.fOutputAssembly = std::make_unique<std::ofstream>(dest);

        auto fmt = CxxKit::current_date();

        (*kState.fOutputAssembly) << "# Path: " << src_file << "\n";
        (*kState.fOutputAssembly) << "# Language: MP-UX Assembly\n";
        (*kState.fOutputAssembly) << "# Build Date: " << fmt << "\n\n";

        ParserKit::SyntaxLeafList syntax;

        kState.fSyntaxTreeList.push_back(syntax);
        kState.fSyntaxTree = &kState.fSyntaxTreeList[kState.fSyntaxTreeList.size() - 1];

        std::string source;

        while (std::getline(src_fp, source))
        {
            kCompilerBackend->Compile(source.c_str(), src.CData());
        }

        if (kAcceptableErrors > 0)
            return -1;

        std::vector<std::string> lines;
        
        struct scope_type
        {
            std::vector<std::string> vals;
            int reg_cnt;
            int id;

            bool operator==(const scope_type& typ) { return typ.id == id; }
        };

        std::vector<scope_type> scope;
        bool found_type = false;
        bool is_pointer = false;
        bool found_expr = false;
        bool found_func = false;

        for (auto& leaf : kState.fSyntaxTree->fLeafList)
        {
            if (leaf.fUserData == "{")
            {
                scope.push_back({});
            }

            if (leaf.fUserData == "{")
            {
                scope.pop_back();
            }

            if (leaf.fUserData == "int" ||
                leaf.fUserData == "long" ||
                leaf.fUserData == "unsigned" ||
                leaf.fUserData == "short" ||
                leaf.fUserData == "char" ||
                leaf.fUserData == "struct" ||
                leaf.fUserData == "class")
            {
                found_type = true;
            }

            if (leaf.fUserData == "(")
            {
                if (found_type)
                {
                    found_expr = true;
                    found_type = false;
                    is_pointer = false;
                }
            }

            if (leaf.fUserData == ")")
            {
                if (found_expr)
                {
                    found_expr = false;
                    is_pointer = false;
                }
            }

            if (leaf.fUserData == ",")
            {
                if (is_pointer)
                {
                    is_pointer = false;
                }
            }

            if (leaf.fUserData == "*")
            {
                if (found_type && !found_expr)
                    is_pointer = true;
            }

            if (leaf.fUserData == "=")
            {
                auto& front = scope.front();

                if (found_type)
                {
                    std::string reg = "r";
                    reg += std::to_string(front.reg_cnt);
                    ++front.reg_cnt;
                    
                    leaf.fUserValue = !is_pointer ? "ldw %s, %s1\n" : "lda %s, %s1\n";

                    for (auto& ln : lines)
                    {
                        if (ln.find(leaf.fUserData) != std::string::npos &&
                            ln.find(";") != std::string::npos)
                        {
                            auto val = ln.substr(ln.find(leaf.fUserData) + leaf.fUserData.size());
                            val.erase(val.find(";"), 1);

                            leaf.fUserValue.replace(leaf.fUserValue.find("%s1"), strlen("%s1"), val);
                        }
                    }

                    leaf.fUserValue.replace(leaf.fUserValue.find("%s"), strlen("%s"), reg);
                }

                is_pointer = false;
                found_type = false;
            }

            if (leaf.fUserData == "return")
            {
                leaf.fUserValue = "ldw r19, %s\njlr";

                if (!lines.empty())
                {
                    for (auto& ln : lines)
                    {
                        if (ln.find(leaf.fUserData) != std::string::npos &&
                            ln.find(";") != std::string::npos)
                        {
                            auto val = ln.substr(ln.find(leaf.fUserData) + leaf.fUserData.size());
                            val.erase(val.find(";"), 1);

                            leaf.fUserValue.replace(leaf.fUserValue.find("%s"), strlen("%s"), val);
                        }
                    }
                }
                else
                {
                    leaf.fUserValue.replace(leaf.fUserValue.find("%s"), strlen("%s"), "0");
                }

                continue;
            }

            lines.push_back(leaf.fUserData);
        }

        for (auto& leaf : kState.fSyntaxTree->fLeafList)
        {
            (*kState.fOutputAssembly) << leaf.fUserValue;
        }

        kState.fSyntaxTree = nullptr;

        kState.fOutputAssembly->flush();
        kState.fOutputAssembly.reset();

        return kOk;
    }

};

/////////////////////////////////////////////////////////////////////////////////////////

#define kPrintF printf
#define kSplashCxx() kPrintF(kWhite "%s\n", "X64000 C compiler, v1.13, (c) WestCo")

static void cxx_print_help()
{
    kSplashCxx();
    kPrintF(kWhite "--asm={MACHINE}: %s\n", "Compile to a specific assembler syntax. (masm)");
    kPrintF(kWhite "--compiler={COMPILER}: %s\n", "Select compiler engine (builtin -> vanhalen++).");
}

/////////////////////////////////////////////////////////////////////////////////////////

#define kExt ".c"

int main(int argc, char** argv)
{
    kKeywords.push_back("auto");
    kKeywords.push_back("else");
    kKeywords.push_back("break");
    kKeywords.push_back("switch");
    kKeywords.push_back("enum");
    kKeywords.push_back("register");
    kKeywords.push_back("do");
    kKeywords.push_back("return");
    kKeywords.push_back("if");
    kKeywords.push_back("default");
    kKeywords.push_back("struct");
    kKeywords.push_back("_Packed");
    kKeywords.push_back("extern");
    kKeywords.push_back("volatile");
    kKeywords.push_back("static");
    kKeywords.push_back("for");
    kKeywords.push_back("class");
    kKeywords.push_back("{");
    kKeywords.push_back("}");
    kKeywords.push_back("(");
    kKeywords.push_back(")");
    kKeywords.push_back("char");
    kKeywords.push_back("int");
    kKeywords.push_back("short");
    kKeywords.push_back("long");
    kKeywords.push_back("float");
    kKeywords.push_back("double");
    kKeywords.push_back("unsigned");
    kKeywords.push_back("__export__");
    kKeywords.push_back("__packed__");
    kKeywords.push_back("namespace");
    kKeywords.push_back("while");
    kKeywords.push_back("sizeof");
    kKeywords.push_back("private");
    kKeywords.push_back("->");
    kKeywords.push_back(".");
    kKeywords.push_back("::");
    kKeywords.push_back("*");
    kKeywords.push_back("+");
    kKeywords.push_back("-");
    kKeywords.push_back("/");
    kKeywords.push_back("=");
    kKeywords.push_back("==");
    kKeywords.push_back("!=");
    kKeywords.push_back(">=");
    kKeywords.push_back("<=");
    kKeywords.push_back(">");
    kKeywords.push_back("<");
    kKeywords.push_back(":");
    kKeywords.push_back(",");
    kKeywords.push_back(";");
    kKeywords.push_back("public");
    kKeywords.push_back("protected");

    bool skip = false;

    for (auto index = 1UL; index < argc; ++index)
    {
        if (skip)
        {
            skip = false;
            continue;
        }

        if (argv[index][0] == '-')
        {
            if (strcmp(argv[index], "-v") == 0 ||
                strcmp(argv[index], "--version") == 0)
            {
                kSplashCxx();
                return kOk;
            }

            if (strcmp(argv[index], "-verbose") == 0)
            {
                kState.kVerbose = true;

                continue;
            }

            if (strcmp(argv[index], "-h") == 0 ||
                strcmp(argv[index], "--help") == 0)
            {
                cxx_print_help();

                return kOk;
            }

            if (strcmp(argv[index], "--dialect") == 0)
            {
                if (kCompilerBackend)
                    std::cout << kCompilerBackend->Language() << "\n";

                return kOk;
            }

            if (strcmp(argv[index], "--asm=masm") == 0)
            {
                delete kFactory.Unmount();

                kFactory.Mount(new AssemblyMountpointClang());
                kMachine = CxxKit::AssemblyFactory::kArchRISCV;

                continue;
            }

            if (strcmp(argv[index], "--compiler=vanhalen") == 0)
            {
                if (!kCompilerBackend)
                    kCompilerBackend = new CompilerBackendClang();

                continue;
            }

            if (strcmp(argv[index], "-fmax-exceptions") == 0)
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

            std::string err = "Unknown command: ";
            err += argv[index];

            detail::print_error(err, "ccplus");

            continue;
        }

        kFileList.emplace_back(argv[index]);

        CxxKit::StringView srcFile = CxxKit::StringBuilder::Construct(argv[index]);

        if (strstr(argv[index], kExt) == nullptr)
        {
            if (kState.kVerbose)
            {
                std::cerr << argv[index] << " is not a valid C source.\n";
            }

            return -1;
        }

        if (kFactory.Compile(srcFile, kMachine) != kOk)
            return -1;
    }

    return kOk;
}
