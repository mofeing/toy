module;

#include <charconv>
#include <coroutine>
#include <iostream>
#include <istream>
#include <limits>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>

export module lexer;

import generator;

namespace toy::lexer {

export struct Token {
    std::string_view text;
    Token(std::string_view text) : text{text} {}
    virtual ~Token() = default;

    bool operator==(const Token& other) const { return this->text == other.text; }
    bool operator!=(const Token& other) const { return !(*this == other); }
};

export struct Eof : public Token {
    Eof() : Token{""} {}
};

export struct Semicolon : public Token {
    Semicolon() : Token{";"} {}
};

export struct ParentheseOpen : public Token {
    ParentheseOpen() : Token{"("} {}
};

export struct ParentheseClose : public Token {
    ParentheseClose() : Token{")"} {}
};

export struct BracketOpen : public Token {
    BracketOpen() : Token{"{"} {}
};

export struct BracketClose : public Token {
    BracketClose() : Token{"}"} {}
};

export struct SbracketOpen : public Token {
    SbracketOpen() : Token{"["} {}
};

export struct SbracketClose : public Token {
    SbracketClose() : Token{"]"} {}
};

export struct Return : public Token {
    Return() : Token{"return"} {}
};

export struct Var : public Token {
    Var() : Token{"var"} {}
};

export struct Def : public Token {
    Def() : Token{"def"} {}
};

export struct Identifier : public Token {
    Identifier(std::string_view text) : Token{text} {}
};

export struct Number : public Token {
    double value;
    Number(std::string_view text) : Token{text} { value = std::stod(std::string{text}); }
};

export struct token_iterator {
    token_iterator(std::istream& is) {}
};

export cppcoro::generator<Token> lex(std::istream& is) {
    while (true) {
        // skip whitespace and newlines
        while (std::isspace(is.peek())) { is.get(); }

        // finish on EOF
        if (is.eof()) {
            co_yield Eof{};
            break;
        }

        // skip comment
        else if (static_cast<char>(is.peek()) == '#') {
            // TODO may fail if comment is last line
            is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }

        // match identifier
        else if (std::isalpha(is.peek())) {
            std::string match;
            while (std::isalnum(is.peek())) { match += is.get(); }

            if (match == "return") co_yield Return{};
            else if (match == "def") co_yield Def{};
            else if (match == "var") co_yield Var{};
            else co_yield Identifier{match};
        }

        // match number
        else if (std::isdigit(is.peek()) || static_cast<char>(is.peek()) == '.') {
            std::string match;
            do { match += is.get(); } while (std::isdigit(is.peek()) || static_cast<char>(is.peek()) == '.');

            co_yield Number{match};
        }

        else if (static_cast<char>(is.peek()) == ';') {
            is.get();
            co_yield Semicolon{};
        }

        else if (static_cast<char>(is.peek()) == '(') {
            is.get();
            co_yield ParentheseOpen{};
        }

        else if (static_cast<char>(is.peek()) == ')') {
            is.get();
            co_yield ParentheseClose{};
        }

        else if (static_cast<char>(is.peek()) == '{') {
            is.get();
            co_yield BracketOpen{};
        }

        else if (static_cast<char>(is.peek()) == '}') {
            is.get();
            co_yield BracketClose{};
        }

        else if (static_cast<char>(is.peek()) == '[') {
            is.get();
            co_yield SbracketOpen{};
        }

        else if (static_cast<char>(is.peek()) == ']') {
            is.get();
            co_yield SbracketClose{};
        }

        // default: match single character
        else
            co_yield Token{std::string{static_cast<char>(is.get())}};
    }
}

}  // namespace toy::lexer
