#ifndef TACO_EXPR_PRINTER_H
#define TACO_EXPR_PRINTER_H

#include <ostream>
#include "taco/expr/expr_visitor.h"

namespace taco {

class ExprPrinter : public ExprVisitorStrict {
public:
  ExprPrinter(std::ostream& os);

  void print(const IndexExpr& expr);

  using ExprVisitorStrict::visit;

  void visit(const AccessNode*);
  void visit(const NegNode*);
  void visit(const SqrtNode*);
  void visit(const AddNode*);
  void visit(const SubNode*);
  void visit(const MulNode*);
  void visit(const DivNode*);
  void visit(const IntImmNode*);
  void visit(const FloatImmNode*);
  void visit(const ComplexImmNode*);
  void visit(const UIntImmNode*);
  void visit(const ReductionNode*);

private:
  std::ostream& os;

  enum class Precedence {
    ACCESS = 2,
    FUNC = 2,
    REDUCTION = 2,
    NEG = 3,
    MUL = 5,
    DIV = 5,
    ADD = 6,
    SUB = 6,
    TOP = 20
  };
  Precedence parentPrecedence;

  template <typename Node> void visitBinary(Node op, Precedence p);
  template <typename Node> void visitImmediate(Node op);
};

}
#endif
