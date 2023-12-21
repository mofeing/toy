module;

#include <coroutine>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

export module parser;

import generator;
import lexer;

using toy::lexer::Token;

namespace toy {

export struct VarType {
    std::vector<int64_t> shape;
};

export struct Expr {
    virtual ~Expr() = default;
};

export using Block = std::vector<std::unique_ptr<Expr>>;

export struct Number : public Expr {
    double value;

    Number(double value) : value{value} {}

    static bool classof(const Expr* expr) {}
};

export struct Literal : public Expr {
    std::vector<std::unique_ptr<Expr>> values;
    std::vector<int64_t> dims;

    Literal(std::vector<std::unique_ptr<Expr>> values, std::vector<int64_t> dims)
        : values{std::move(values)}, dims{std::move(dims)} {}
};

export struct VariableReference : public Expr {
    std::string name;

    VariableReference(std::string name) : name{name} {}
};

export struct VariableDeclaration : public Expr {
    std::string name;
    VarType type;
    std::unique_ptr<Expr> value;

    VariableDeclaration(std::string name, VarType type, std::unique_ptr<Expr>&& value)
        : name{name}, type{type}, value{std::move(value)} {}
};

export struct Return : public Expr {
    std::optional<std::unique_ptr<Expr>> expr;

    Return(std::unique_ptr<Expr>&& expr) : expr{std::move(expr)} {}
};

export struct BinaryOperator : public Expr {
    std::string op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;

    BinaryOperator(std::string op, std::unique_ptr<Expr>&& lhs, std::unique_ptr<Expr>&& rhs)
        : op{op}, lhs{std::move(lhs)}, rhs{std::move(rhs)} {}
};

export struct Call : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;

    Call(const std::string& callee, std::vector<std::unique_ptr<Expr>> args) : callee{callee}, args{std::move(args)} {}
};

export struct Print : public Expr {
    std::unique_ptr<Expr> arg;

    Print(std::unique_ptr<Expr>&& arg) : arg{std::move(arg)} {}
};

export struct Prototype : public Expr {
    std::string _name;
    std::vector<std::string> args;

    Prototype(const std::string& name, std::vector<std::string> args) : _name{name}, args{std::move(args)} {}
    const std::string& name() const { return this->_name; }
};

export struct Function : public Expr {
    std::unique_ptr<Prototype> proto;
    std::unique_ptr<Block> body;

    Function(std::unique_ptr<Prototype>&& proto, std::unique_ptr<Block>&& body)
        : proto{std::move(proto)}, body{std::move(body)} {}
};

export struct Module : public Expr {
    std::vector<Function> functions;

    Module(std::vector<Function> functions) : functions{std::move(functions)} {}
    auto begin() { return functions.begin(); }
    auto end() { return functions.end(); }
};

template <typename Iterator>
std::unique_ptr<Expr> parse_expr(Iterator it);

template <typename Iterator>
std::unique_ptr<Expr> parse_return(Iterator it) {
    it++;  // eat 'return'

    if (*it == lexer::Token{";"}) return std::make_unique<Return>(nullptr);

    auto expr = parse_expr(it);

    if (*it != lexer::Token{";"}) throw std::runtime_error{"expected ';'"};
    it++;  // eat ';'

    return std::make_unique<Return>(std::move(expr));
}

template <typename Iterator>
std::unique_ptr<Expr> parse_number(Iterator it) {
    auto result = std::make_unique<Number>(std::stod(std::string(it->text)));
    it++;  // eat number
    return std::move(result);
}

template <typename Iterator>
std::unique_ptr<Expr> parse_tensor_literal(Iterator it) {
    if (*it != lexer::Token{"["}) throw std::runtime_error{"expected '[' to begin tensor literal"};
    it++;  // eat '('

    std::vector<std::unique_ptr<Expr>> values;
    while (true) {
        // nested array
        if (*it == lexer::Token{"["}) {
            auto expr = parse_tensor_literal(it);
            if (expr == nullptr) return nullptr;
            values.push_back(std::move(expr));
        }
        // number
        else if (auto* number = dynamic_cast<lexer::Number*>(&*it))
            values.push_back(parse_number(it));
        else throw std::runtime_error{"expected a number literal or a nested array"};

        // end of this level on ']'
        if (*it == Token{"]"}) break;

        // ensure elements are separated by a comma
        if (*it != Token{","}) throw std::runtime_error("expected ']' or ',' in tensor literal");
        it++;  // eat ,
    }

    if (values.empty()) throw std::runtime_error("empty tensor literals are not supported");
    it++;  // eat ]

    std::vector<int64_t> dims;
    dims.push_back(values.size());

    // if nested array, process and ensure dims are uniform
    if (std::any_of(values.begin(), values.end(), [](auto& expr) {
            return dynamic_cast<Literal*>(expr.get()) != nullptr;
        })) {
        auto* firstlit = dynamic_cast<Literal*>(values.front().get());
        if (firstlit == nullptr)
            throw std::runtime_error("expected uniformly well-nested dimensions inside tensor literal");

        // append nested dims to current level
        auto firstdims = firstlit->dims;
        std::copy(firstdims.begin(), firstdims.end(), std::back_inserter(dims));

        // sanity check that shape is uniform across all elements of the list
        for (auto& expr : values) {
            auto* exprlit = dynamic_cast<Literal*>(expr.get());
            if (exprlit == nullptr)
                throw std::runtime_error("expected uniformly well-nested dimensions inside tensor literal");

            if (exprlit->dims != firstdims)
                throw std::runtime_error("expected uniformly well-nested dimensions inside tensor literal");
        }
    }

    return std::make_unique<Literal>(std::move(values), std::move(dims));
}

template <typename Iterator>
std::unique_ptr<Expr> parse_parenthesis(Iterator it) {
    if (*it != lexer::Token{"("}) throw std::runtime_error{"expected ')'"};
    it++;  // eat '('

    auto expr = parse_expr(it);

    if (*it != lexer::Token{")"}) throw std::runtime_error{"expected ')'"};
    it++;  // eat ')'

    return expr;
}

template <typename Iterator>
std::unique_ptr<Expr> parse_identifier(Iterator it) {
    auto name = std::string(it->text);
    it++;  // eat identifier

    if (*it != lexer::Token{"("}) return std::make_unique<VariableReference>(name);

    it++;  // eat '('
    std::vector<std::unique_ptr<Expr>> args;
    while (*it != lexer::Token{")"}) {
        if (auto arg = parse_expr(it)) args.push_back(std::move(arg));
        else throw std::runtime_error{"expected expression"};
        if (*it == lexer::Token{","}) it++;
    }
    it++;  // eat ')'

    if (name == "print") {
        if (args.size() != 1) throw std::runtime_error("'print' expects 1 argument");
        return std::make_unique<Print>(std::move(args[0]));
    }

    return std::make_unique<Call>(name, std::move(args));
}

template <typename Iterator>
std::unique_ptr<Expr> parse_primary(Iterator it) {
    if (dynamic_cast<lexer::ParentheseOpen*>(&*it) != nullptr) return parse_parenthesis(it);
    else if (dynamic_cast<lexer::SbracketOpen*>(&*it) != nullptr) return parse_tensor_literal(it);
    else if (dynamic_cast<lexer::Semicolon*>(&*it) != nullptr) return nullptr;
    else if (dynamic_cast<lexer::BracketClose*>(&*it) != nullptr) return nullptr;
    else if (dynamic_cast<lexer::Identifier*>(&*it) != nullptr) return parse_identifier(it);
    else if (dynamic_cast<lexer::Number*>(&*it) != nullptr) return parse_number(it);
    else throw std::runtime_error{"unknown token: " + std::string(it->text)};
}

const std::map<std::string, int> operator_precedence_map{
    {"<", 10},
    {"+", 20},
    {"-", 20},
    {"*", 40},
};

template <typename Iterator>
int operator_precedence(Iterator it) {
    try {
        return operator_precedence_map.at(std::string(it->text));
    } catch (std::out_of_range& e) { return -1; }
}

template <typename Iterator>
std::unique_ptr<Expr> parse_binop_rhs(int expr_precedence, std::unique_ptr<Expr> lhs, Iterator it) {
    while (true) {
        auto token_precedence = operator_precedence(it);
        if (token_precedence < expr_precedence) return lhs;

        auto binop = std::string(it->text);
        it++;  // eat binop

        auto rhs = parse_primary(it);
        if (rhs == nullptr) throw std::runtime_error("expected expression to complete binary operator");

        auto next_precedence = operator_precedence(it);
        if (token_precedence < next_precedence) {
            rhs = parse_binop_rhs(token_precedence + 1, std::move(rhs), it);
            if (rhs == nullptr) return nullptr;
        }

        lhs = std::make_unique<BinaryOperator>(binop, std::move(lhs), std::move(rhs));
    }
}

template <typename Iterator>
std::unique_ptr<Expr> parse_expr(Iterator it) {
    auto lhs = parse_primary(it);
    return parse_binop_rhs(0, std::move(lhs), it);
}

template <typename Iterator>
std::unique_ptr<VarType> parse_type(Iterator it) {
    if (*it != Token{"<"}) throw std::runtime_error("expected '<' to begin type");
    it++;  // eat <

    auto type = std::make_unique<VarType>();

    while (auto* number = dynamic_cast<lexer::Number*>(&*it)) {
        type->shape.push_back(number->value);
        it++;                         // eat number
        if (*it == Token{","}) it++;  // eat ,
    }

    if (*it != Token{">"}) throw std::runtime_error("expected '>' to end type");
    it++;  // eat >

    return type;
}

template <typename Iterator>
std::unique_ptr<Expr> parse_vardecl(Iterator it) {
    if (dynamic_cast<lexer::Var*>(&*it) == nullptr)
        throw std::runtime_error("expected 'var' to begin variable declaration");
    it++;  // eat var

    auto* name = dynamic_cast<lexer::Identifier*>(&*it);
    if (name == nullptr) throw std::runtime_error("expected identifier in variable declaration");

    std::string id{name->text};
    it++;  // eat id

    std::unique_ptr<VarType> type;
    if (*it == Token{"<"}) {
        type = parse_type(it);
        if (type == nullptr) return nullptr;
    } else type = std::make_unique<VarType>();

    if (*it != Token{"="}) throw std::runtime_error("expected '=' in variable declaration");
    it++;  // eat =

    auto expr = parse_expr(it);

    return std::make_unique<VariableDeclaration>(std::move(id), std::move(*type), std::move(expr));
}

template <typename Iterator>
std::unique_ptr<Block> parse_block(Iterator it) {
    if (dynamic_cast<lexer::BracketOpen*>(&*it) == nullptr) throw std::runtime_error("expected '{' to begin block");
    it++;  // eat {

    auto list = std::make_unique<Block>();

    // ignore empty expressions
    while (dynamic_cast<lexer::Semicolon*>(&*it) != nullptr) it++;

    while (dynamic_cast<lexer::BracketClose*>(&*it) == nullptr && dynamic_cast<lexer::Eof*>(&*it) == nullptr) {
        if (dynamic_cast<lexer::Var*>(&*it) != nullptr) {
            auto expr = parse_vardecl(it);
            if (expr == nullptr) return nullptr;
            list->push_back(std::move(expr));

        } else if (dynamic_cast<lexer::Return*>(&*it) != nullptr) {
            auto expr = parse_return(it);
            if (expr == nullptr) return nullptr;
            list->push_back(std::move(expr));

        } else {
            auto expr = parse_expr(it);
            if (expr == nullptr) return nullptr;
            list->push_back(std::move(expr));
        }

        // ensure elements are separated by ';'
        if (dynamic_cast<lexer::Semicolon*>(&*it) == nullptr) throw std::runtime_error("expected ';' after expression");

        // ignore empty expressions
        while (dynamic_cast<lexer::Semicolon*>(&*it) != nullptr) it++;
    }

    if (dynamic_cast<lexer::BracketClose*>(&*it) == nullptr) throw std::runtime_error("expected '}' to close block");
    it++;  // eat }

    return list;
}

template <typename Iterator>
std::unique_ptr<Prototype> parse_prototype(Iterator it) {
    if (dynamic_cast<lexer::Def*>(&*it) == nullptr) throw std::runtime_error{"expected 'def' in prototype"};
    it++;  // eat 'def'

    if (auto* name = dynamic_cast<lexer::Identifier*>(&*it); name == nullptr)
        throw std::runtime_error{"expected function name in prototype"};
    auto name = std::string(it->text);
    it++;  // eat identifier

    if (*it != lexer::Token{"("}) throw std::runtime_error{"expected '(' in prototype"};
    it++;  // eat '('

    std::vector<std::string> args;
    while (auto* arg = dynamic_cast<lexer::Identifier*>(&*it)) {
        args.push_back(std::string(arg->text));
        it++;  // eat identifier
    }

    if (*it != lexer::Token{")"}) throw std::runtime_error{"expected ')' in prototype"};
    it++;  // eat ')'

    return std::make_unique<Prototype>(name, std::move(args));
}

template <typename Iterator>
std::unique_ptr<Function> parse_function(Iterator it) {
    auto proto = parse_prototype(it);
    if (proto == nullptr) return nullptr;

    auto block = parse_block(it);
    if (block == nullptr) return nullptr;

    return std::make_unique<Function>(std::move(proto), std::move(block));
}

export std::unique_ptr<Module> parse(std::istream& is) {
    auto tokens = lexer::lex(is);
    auto it = tokens.begin();

    std::vector<Function> functions;
    while (auto func = parse_function(it)) {
        functions.push_back(std::move(*func));
        if (dynamic_cast<lexer::Eof*>(&*it) != nullptr) break;
    }

    if (dynamic_cast<lexer::Eof*>(&*it) == nullptr) throw std::runtime_error{"parsing error"};

    return std::make_unique<Module>(std::move(functions));
}

}  // namespace toy
