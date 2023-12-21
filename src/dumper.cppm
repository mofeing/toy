module;

#include <iostream>
#include <ranges>

export module dumper;
import parser;

using namespace toy;

class ASTDumper {
   public:
    void dump(Module* node) {
        indent_level++;
        indent();

        std::cerr << "Module" << std::endl;
        for (auto& f : *node) dump(&f);

        indent_level--;
    }

   private:
    // dispatch to appropiate subclass
    void dump(Expr* expr) {
        if (auto* cast_expr = dynamic_cast<BinaryOperator*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<Call*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<Literal*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<Number*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<Print*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<Return*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<VariableDeclaration*>(expr); cast_expr != nullptr)
            this->dump(cast_expr);
        else if (auto* cast_expr = dynamic_cast<VariableReference*>(expr); cast_expr != nullptr) this->dump(cast_expr);
        else {
            indent_level++;
            indent();
            std::cerr << "<unknown Expr>" << std::endl;
            indent_level--;
        }
    }

    void dump(const VarType& type) {
        std::cerr << "<";

        if (type.shape.size() > 0) {
            std::cout << type.shape[0];
            for (auto& arg : type.shape | std::ranges::views::drop(1)) std::cout << ", " << arg;
        }

        std::cerr << ">";
    }

    void dump(VariableDeclaration* vardecl) {
        indent_level++;
        indent();

        std::cerr << "VariableDeclaration " << vardecl->name;
        dump(vardecl->type);
        std::cerr << " " << std::endl;
        dump(vardecl->value.get());

        indent_level--;
    }

    void dump(Block* list) {
        indent_level++;
        indent();

        std::cerr << "Block" << std::endl;
        for (auto& expr : *list) dump(expr.get());

        indent_level--;
    }

    void dump(Number* num) {
        indent_level++;
        indent();

        std::cerr << num->value << std::endl;

        indent_level--;
    }

    void dump(Literal* node) {}

    void dump(VariableReference* node) {
        indent_level++;
        indent();

        std::cerr << "var: " << node->name << std::endl;

        indent_level--;
    }

    void dump(Return* node) {
        indent_level++;
        indent();

        std::cerr << "Return" << std::endl;
        if (node->expr.has_value()) dump(node->expr.value().get());

        indent_level--;
    }

    void dump(BinaryOperator* node) {
        indent_level++;
        indent();

        std::cerr << "BinaryOperator: " << node->op << std::endl;
        dump(node->lhs.get());
        dump(node->rhs.get());

        indent_level--;
    }

    void dump(Call* node) {
        indent_level++;
        indent();

        std::cerr << "Call '" << node->callee << "'" << std::endl;
        for (auto& arg : node->args) dump(arg.get());

        indent_level--;
    }

    void dump(Print* node) {
        indent_level++;
        indent();

        std::cerr << "Print" << std::endl;
        dump(node->arg.get());
        indent();
        std::cerr << std::endl;

        indent_level--;
    }

    void dump(Prototype* node) {
        indent_level++;
        indent();

        std::cerr << "Proto '" << node->name() << "'" << std::endl;
        indent();
        std::cerr << "Params: [";

        if (node->args.size() > 0) {
            std::cout << node->args[0];
            for (auto& arg : node->args | std::ranges::views::drop(1)) std::cout << ", " << arg;
        }

        std::cerr << "]" << std::endl;

        indent_level--;
    }

    void dump(Function* node) {
        indent_level++;
        indent();

        std::cerr << "Function" << std::endl;
        dump(node->proto.get());
        dump(node->body.get());

        indent_level--;
    }

    void indent() const {
        for (int _ : std::ranges::iota_view(1, indent_level)) std::cerr << "  ";
    }

    int indent_level = 0;
};

namespace toy {
export void dump(Module& module) { ASTDumper().dump(&module); }
}  // namespace toy