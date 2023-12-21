#include <cxxabi.h>
#include <fstream>
#include <iostream>
#include <string>

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/ErrorOr.h>

namespace cl = llvm::cl;

import toy;

std::string demangle(const char* name) {
    int status = -1;
    std::unique_ptr<char, void (*)(void*)> res{abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
    return (status == 0) ? res.get() : name;
}

static cl::opt<std::string> filename{
    cl::Positional,
    cl::desc("<input .toy file>"),
    cl::init("-"),
    cl::value_desc("filename"),
};

enum Action {
    None,
    DumpTokens,
    DumpAST,
};

static cl::opt<enum Action> emit_action{
    "emit",
    cl::desc("Select the desired output kind"),
    cl::values(
        clEnumValN(DumpTokens, "tokens", "output of the lexer"),
        clEnumValN(DumpAST, "ast", "output of the AST dump")
    ),
};

int main(int argc, const char** argv) {
    cl::ParseCommandLineOptions(argc, argv, "toy compiler\n");
    std::ifstream is{filename, std::ios::in};
    if (!is.is_open()) {
        std::cerr << "cannot open '" << filename << "'" << std::endl;
        return 1;
    }

    switch (emit_action) {
        case Action::DumpTokens: {
            auto tokens = toy::lexer::lex(is);
            for (auto& token : tokens)
                std::cout << "[" << demangle(typeid(token).name()) << "] " << token.text << std::endl;
            return 0;
        }

        case Action::DumpAST: {
            auto mod = toy::parse(is);
            if (mod == nullptr) return 1;
            toy::dump(*mod);
            return 0;
        }

        default:
            std::cerr << "No action specified (parsing only?), use -emit=<action>" << std::endl;
    }

    return 0;
}