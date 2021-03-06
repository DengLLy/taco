#include "taco/expr/expr.h"

#include "error/error_checks.h"
#include "error/error_messages.h"
#include "taco/type.h"
#include "taco/format.h"
#include "taco/expr/schedule.h"
#include "taco/expr/expr_nodes.h"
#include "taco/expr/expr_rewriter.h"
#include "taco/expr/expr_printer.h"
#include "taco/util/name_generator.h"

using namespace std;

namespace taco {

// class ExprNode
ExprNode::ExprNode() : operatorSplits(new vector<OperatorSplit>) {
  }

void ExprNode::splitOperator(IndexVar old, IndexVar left, IndexVar right) {
  operatorSplits->push_back(OperatorSplit(this, old, left, right));
}
  
ExprNode::ExprNode(DataType type) : operatorSplits(new vector<OperatorSplit>), dataType(type) {
}

DataType ExprNode::getDataType() const {
  return dataType;
}

const std::vector<OperatorSplit>& ExprNode::getOperatorSplits() const {
  return *operatorSplits;
}


// class IndexExpr
IndexExpr::IndexExpr(TensorVar var) : IndexExpr(new AccessNode(var,{})) {
}

IndexExpr::IndexExpr(long long val) : IndexExpr(new IntImmNode(val)) {
}

IndexExpr::IndexExpr(std::complex<double> val) : IndexExpr(new ComplexImmNode(val)) {
}

IndexExpr::IndexExpr(unsigned long long val) : IndexExpr(new UIntImmNode(val)) {
}

IndexExpr::IndexExpr(double val) : IndexExpr(new FloatImmNode(val)) {
}

void IndexExpr::splitOperator(IndexVar old, IndexVar left, IndexVar right) {
  const_cast<ExprNode*>(this->ptr)->splitOperator(old, left, right);
}
  
DataType IndexExpr::getDataType() const {
  return const_cast<ExprNode*>(this->ptr)->getDataType();
}

void IndexExpr::accept(ExprVisitorStrict *v) const {
  ptr->accept(v);
}

std::ostream& operator<<(std::ostream& os, const IndexExpr& expr) {
  if (!expr.defined()) return os << "IndexExpr()";
  ExprPrinter printer(os);
  printer.print(expr);
  return os;
  
}

struct Equals : public ExprVisitorStrict {
  bool eq = false;
  IndexExpr b;

  bool check(IndexExpr a, IndexExpr b) {
    this->b = b;
    a.accept(this);
    return eq;
  }

  using ExprVisitorStrict::visit;

  void visit(const AccessNode* anode) {
    if (!isa<AccessNode>(b)) {
      eq = false;
      return;
    }

    auto bnode = to<AccessNode>(b);
    if (anode->tensorVar != bnode->tensorVar) {
      eq = false;
      return;
    }
    if (anode->indexVars.size() != anode->indexVars.size()) {
      eq = false;
      return;
    }
    for (size_t i = 0; i < anode->indexVars.size(); i++) {
      if (anode->indexVars[i] != bnode->indexVars[i]) {
        eq = false;
        return;
      }
    }

    eq = true;
  }

  template <class T>
  bool unaryEquals(const T* anode, IndexExpr b) {
    if (!isa<T>(b)) {
      return false;
    }
    auto bnode = to<T>(b);
    if (!equals(anode->a, bnode->a)) {
      return false;
    }
    return true;
  }

  void visit(const NegNode* anode) {
    eq = unaryEquals(anode, b);
  }

  void visit(const SqrtNode* anode) {
    eq = unaryEquals(anode, b);
  }

  template <class T>
  bool binaryEquals(const T* anode, IndexExpr b) {
    if (!isa<T>(b)) {
      return false;
    }
    auto bnode = to<T>(b);
    if (!equals(anode->a, bnode->a) || !equals(anode->b, bnode->b)) {
      return false;
    }
    return true;
  }

  void visit(const AddNode* anode) {
    eq = binaryEquals(anode, b);
  }

  void visit(const SubNode* anode) {
    eq = binaryEquals(anode, b);
  }

  void visit(const MulNode* anode) {
    eq = binaryEquals(anode, b);
  }

  void visit(const DivNode* anode) {
    eq = binaryEquals(anode, b);
  }

  void visit(const ReductionNode* anode) {
    if (!isa<ReductionNode>(b)) {
      eq = false;
      return;
    }
    auto bnode = to<ReductionNode>(b);
    if (!(equals(anode->op, bnode->op) && equals(anode->a, bnode->a))) {
      eq = false;
      return;
    }
    eq = true;
  }

  template <class T>
  bool immediateEquals(const T* anode, IndexExpr b) {
    if (!isa<T>(b)) {
      return false;
    }
    auto bnode = to<T>(b);
    if (anode->val != bnode->val) {
      return false;
    }
    return true;
  }

  void visit(const IntImmNode* anode) {
    eq = immediateEquals(anode, b);

  }

  void visit(const FloatImmNode* anode) {
    eq = immediateEquals(anode, b);
  }

  void visit(const ComplexImmNode* anode) {
    eq = immediateEquals(anode, b);
  }

  void visit(const UIntImmNode* anode) {
    eq = immediateEquals(anode, b);
  }
};

bool equals(IndexExpr a, IndexExpr b) {
  if (!a.defined() && !b.defined()) {
    return true;
  }
  if ((a.defined() && !b.defined()) || (!a.defined() && b.defined())) {
    return false;
  }
  return Equals().check(a,b);
}

IndexExpr operator-(const IndexExpr& expr) {
  return new NegNode(expr.ptr);
}

IndexExpr operator+(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new AddNode(lhs, rhs);
}

IndexExpr operator-(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new SubNode(lhs, rhs);
}

IndexExpr operator*(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new MulNode(lhs, rhs);
}

IndexExpr operator/(const IndexExpr& lhs, const IndexExpr& rhs) {
  return new DivNode(lhs, rhs);
}


// class Access
Access::Access(const Node* n) : IndexExpr(n) {
}

Access::Access(const TensorVar& tensor, const std::vector<IndexVar>& indices)
    : Access(new Node(tensor, indices)) {
}

const Access::Node* Access::getPtr() const {
  return static_cast<const Node*>(ptr);
}

const TensorVar& Access::getTensorVar() const {
  return getPtr()->tensorVar;
}

const std::vector<IndexVar>& Access::getIndexVars() const {
  return getPtr()->indexVars;
}

void Access::operator=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  taco_uassert(!result.getIndexExpr().defined()) << "Cannot reassign " <<result;
  const_cast<AccessNode*>(getPtr())->setIndexExpression(expr, false);
}

void Access::operator=(const Access& expr) {
  operator=(static_cast<IndexExpr>(expr));
}

void Access::operator+=(const IndexExpr& expr) {
  TensorVar result = getTensorVar();
  taco_uassert(!result.getIndexExpr().defined()) << "Cannot reassign " <<result;
  // TODO: check that result format is dense. For now only support accumulation
  /// into dense. If it's not dense, then we can insert an operator split.
  const_cast<AccessNode*>(getPtr())->setIndexExpression(expr, true);
}

void Access::operator+=(const Access& expr) {
  operator+=(static_cast<IndexExpr>(expr));
}


// class Sum
Reduction::Reduction(const Node* n) : IndexExpr(n) {
}

Reduction::Reduction(const IndexExpr& op, const IndexVar& var,
                     const IndexExpr& expr)
    : Reduction(new Node(op, var, expr)) {

}

Reduction ReductionProxy::operator()(const IndexExpr& expr) {
  return Reduction(op, var, expr);
}

ReductionProxy sum(IndexVar indexVar) {
  return ReductionProxy(new AddNode, indexVar);
}

// class IndexVar
struct IndexVar::Content {
  string name;
};

IndexVar::IndexVar() : IndexVar(util::uniqueName('i')) {}

IndexVar::IndexVar(const std::string& name) : content(new Content) {
  content->name = name;
}

std::string IndexVar::getName() const {
  return content->name;
}

bool operator==(const IndexVar& a, const IndexVar& b) {
  return a.content == b.content;
}

bool operator<(const IndexVar& a, const IndexVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const IndexVar& var) {
  return os << var.getName();
}


// class TensorVar
struct TensorVar::Content {
  string name;
  Type type;
  Format format;

  vector<IndexVar> freeVars;
  IndexExpr indexExpr;
  bool accumulate;

  Schedule schedule;
};

TensorVar::TensorVar() : TensorVar(Type()) {
}

TensorVar::TensorVar(const Type& type) : TensorVar(type, Dense) {
}

TensorVar::TensorVar(const std::string& name, const Type& type)
    : TensorVar(name, type, Dense) {
}

TensorVar::TensorVar(const Type& type, const Format& format)
    : TensorVar(util::uniqueName('A'), type, format) {
}

TensorVar::TensorVar(const string& name, const Type& type, const Format& format)
    : content(new Content) {
  content->name = name;
  content->type = type;
  content->format = format;
}

std::string TensorVar::getName() const {
  return content->name;
}

int TensorVar::getOrder() const {
  return content->type.getShape().getOrder();
}

const Type& TensorVar::getType() const {
  return content->type;
}

const Format& TensorVar::getFormat() const {
  return content->format;
}

const std::vector<IndexVar>& TensorVar::getFreeVars() const {
  return content->freeVars;
}

const IndexExpr& TensorVar::getIndexExpr() const {
  return content->indexExpr;
}

bool TensorVar::isAccumulating() const {
  return content->accumulate;
}

const Schedule& TensorVar::getSchedule() const {
  struct GetSchedule : public ExprVisitor {
    using ExprVisitor::visit;
    Schedule schedule;
    void visit(const BinaryExprNode* expr) {
      for (auto& operatorSplit : expr->getOperatorSplits()) {
        schedule.addOperatorSplit(operatorSplit);
      }
    }
  };
  GetSchedule getSchedule;
  content->schedule.clearOperatorSplits();
  getSchedule.schedule = content->schedule;
  getIndexExpr().accept(&getSchedule);
  return content->schedule;
}

void TensorVar::setName(std::string name) {
  content->name = name;
}

void TensorVar::setIndexExpression(vector<IndexVar> freeVars,
                                   IndexExpr indexExpr, bool accumulate) {
  auto shape = getType().getShape();
  taco_uassert(error::dimensionsTypecheck(freeVars, indexExpr, shape))
      << error::expr_dimension_mismatch << " "
      << error::dimensionTypecheckErrors(freeVars, indexExpr, shape);

    taco_uassert(verify(indexExpr, freeVars))
      << error::expr_einsum_missformed << endl
      << getName() << "(" << util::join(getFreeVars()) << ") "
      << (accumulate ? "+=" : "=") << " "
      << indexExpr;

  // The following are index expressions the implementation doesn't currently
  // support, but that are planned for the future.
  taco_uassert(!error::containsTranspose(this->getFormat(), freeVars, indexExpr))
      << error::expr_transposition;
  taco_uassert(!error::containsDistribution(freeVars, indexExpr))
      << error::expr_distribution;

  content->freeVars = freeVars;
  content->indexExpr = indexExpr;
  content->accumulate = accumulate;
}

const Access TensorVar::operator()(const std::vector<IndexVar>& indices) const {
  taco_uassert((int)indices.size() == getOrder()) <<
      "A tensor of order " << getOrder() << " must be indexed with " <<
      getOrder() << " variables, but is indexed with:  " << util::join(indices);
  return Access(new AccessNode(*this, indices));
}

Access TensorVar::operator()(const std::vector<IndexVar>& indices) {
  taco_uassert((int)indices.size() == getOrder()) <<
      "A tensor of order " << getOrder() << " must be indexed with " <<
      getOrder() << " variables, but is indexed with:  " << util::join(indices);
  return Access(new AccessNode(*this, indices));
}

void TensorVar::operator=(const IndexExpr& expr) {
  taco_uassert(getOrder() == 0)
      << "Must use index variable on the left-hand-side when assigning an "
      << "expression to a non-scalar tensor.";
  taco_uassert(!getIndexExpr().defined()) << "Cannot reassign " << *this;
  setIndexExpression(getFreeVars(), expr);
}

bool operator==(const TensorVar& a, const TensorVar& b) {
  return a.content == b.content;
}

bool operator<(const TensorVar& a, const TensorVar& b) {
  return a.content < b.content;
}

std::ostream& operator<<(std::ostream& os, const TensorVar& var) {
  return os << var.getName() << " : " << var.getType();
}


// functions
vector<IndexVar> getIndexVars(const IndexExpr& expr) {
  vector<IndexVar> indexVars;
  set<IndexVar> seen;
  match(expr,
    function<void(const AccessNode*)>([&](const AccessNode* op) {
      for (auto& var : op->indexVars) {
        if (!util::contains(seen, var)) {
          seen.insert(var);
          indexVars.push_back(var);
        }
      }
    })
  );
  return indexVars;
}

set<IndexVar> getIndexVars(const TensorVar& tensor) {
  auto indexVars = util::combine(tensor.getFreeVars(),
                                 getIndexVars(tensor.getIndexExpr()));
  return set<IndexVar>(indexVars.begin(), indexVars.end());
}

map<IndexVar,Dimension> getIndexVarRanges(const TensorVar& tensor) {
  map<IndexVar, Dimension> indexVarRanges;

  auto& freeVars = tensor.getFreeVars();
  auto& type = tensor.getType();
  for (size_t i = 0; i < freeVars.size(); i++) {
    indexVarRanges.insert({freeVars[i], type.getShape().getDimension(i)});
  }

  match(tensor.getIndexExpr(),
    function<void(const AccessNode*)>([&indexVarRanges](const AccessNode* op) {
      auto& tensor = op->tensorVar;
      auto& vars = op->indexVars;
      auto& type = tensor.getType();
      for (size_t i = 0; i < vars.size(); i++) {
        indexVarRanges.insert({vars[i], type.getShape().getDimension(i)});
      }
    })
  );
  
  return indexVarRanges;
}

struct Simplify : public ExprRewriterStrict {
public:
  Simplify(const set<Access>& zeroed) : zeroed(zeroed) {}

private:
  using ExprRewriterStrict::visit;

  set<Access> zeroed;
  void visit(const AccessNode* op) {
    if (util::contains(zeroed, op)) {
      expr = IndexExpr();
    }
    else {
      expr = op;
    }
  }

  template <class T>
  IndexExpr visitUnaryOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    if (!a.defined()) {
      return IndexExpr();
    }
    else if (a == op->a) {
      return op;
    }
    else {
      return new T(a);
    }
  }

  void visit(const NegNode* op) {
    expr = visitUnaryOp(op);
  }

  void visit(const SqrtNode* op) {
    expr = visitUnaryOp(op);
  }

  template <class T>
  IndexExpr visitDisjunctionOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    IndexExpr b = rewrite(op->b);
    if (!a.defined() && !b.defined()) {
      return IndexExpr();
    }
    else if (!a.defined()) {
      return b;
    }
    else if (!b.defined()) {
      return a;
    }
    else if (a == op->a && b == op->b) {
      return op;
    }
    else {
      return new T(a, b);
    }
  }

  template <class T>
  IndexExpr visitConjunctionOp(const T *op) {
    IndexExpr a = rewrite(op->a);
    IndexExpr b = rewrite(op->b);
    if (!a.defined() || !b.defined()) {
      return IndexExpr();
    }
    else if (a == op->a && b == op->b) {
      return op;
    }
    else {
      return new T(a, b);
    }
  }

  void visit(const AddNode* op) {
    expr = visitDisjunctionOp(op);
  }

  void visit(const SubNode* op) {
    expr = visitDisjunctionOp(op);
  }

  void visit(const MulNode* op) {
    expr = visitConjunctionOp(op);
  }

  void visit(const DivNode* op) {
    expr = visitConjunctionOp(op);
  }

  void visit(const ReductionNode* op) {
    IndexExpr a = rewrite(op->a);
    if (!a.defined()) {
      expr = IndexExpr();
    }
    else if (a == op->a) {
      expr = op;
    }
    else {
      expr = new ReductionNode(op->op, op->var, a);
    }
  }

  void visit(const IntImmNode* op) {
    expr = op;
  }

  void visit(const FloatImmNode* op) {
    expr = op;
  }

  void visit(const UIntImmNode* op) {
    expr = op;
  }

  void visit(const ComplexImmNode* op) {
    expr = op;
  }
};

IndexExpr simplify(const IndexExpr& expr, const set<Access>& zeroed) {
  return Simplify(zeroed).rewrite(expr);
}

set<IndexVar> getVarsWithoutReduction(const IndexExpr& expr) {
  struct GetVarsWithoutReduction : public ExprVisitor {
    set<IndexVar> indexvars;

    set<IndexVar> get(const IndexExpr& expr) {
      indexvars.clear();
      expr.accept(this);
      return indexvars;
    }

    using ExprVisitorStrict::visit;

    void visit(const AccessNode* op) {
      indexvars.insert(op->indexVars.begin(), op->indexVars.end());
    }

    void visit(const ReductionNode* op) {
      indexvars.erase(op->var);
    }
  };
  return GetVarsWithoutReduction().get(expr);
}

bool verify(const IndexExpr& expr, const std::vector<IndexVar>& free){
  set<IndexVar> freeVars(free.begin(), free.end());
  for (auto& var : getVarsWithoutReduction(expr)) {
    if (!util::contains(freeVars, var)) {
      return false;
    }
  }
  return true;
}

bool verify(const TensorVar& var) {
  return verify(var.getIndexExpr(), var.getFreeVars());
}

bool doesEinsumApply(IndexExpr expr) {
  struct VerifyEinsum : public ExprVisitor {
    bool isEinsum;
    bool mulnodeVisited;

    bool verify(IndexExpr expr) {
      // Einsum until proved otherwise
      isEinsum = true;

      // Additions are not allowed under the first multplication
      mulnodeVisited = false;

      expr.accept(this);
      return isEinsum;
    }

    using ExprVisitor::visit;

    void visit(const AddNode* node) {
      if (mulnodeVisited) {
        isEinsum = false;
        return;
      }
      node->a.accept(this);
      node->b.accept(this);
    }

    void visit(const SubNode* node) {
      if (mulnodeVisited) {
        isEinsum = false;
        return;
      }
      node->a.accept(this);
      node->b.accept(this);
    }

    void visit(const MulNode* node) {
      bool topMulNode = !mulnodeVisited;
      mulnodeVisited = true;
      node->a.accept(this);
      node->b.accept(this);
      if (topMulNode) {
        mulnodeVisited = false;
      }
    }

    void visit(const BinaryExprNode* node) {
      isEinsum = false;
    }

    void visit(const ReductionNode* node) {
      isEinsum = false;
    }
  };
  return VerifyEinsum().verify(expr);
}

IndexExpr einsum(const IndexExpr& expr, const std::vector<IndexVar>& free) {
  if (!doesEinsumApply(expr)) {
    return IndexExpr();
  }

  struct Einsum : ExprRewriter {
    Einsum(const std::vector<IndexVar>& free) : free(free.begin(), free.end()){}

    std::set<IndexVar> free;
    bool onlyOneTerm;

    IndexExpr addReductions(IndexExpr expr) {
      auto vars = getIndexVars(expr);
      for (auto& var : util::reverse(vars)) {
        if (!util::contains(free, var)) {
          expr = sum(var)(expr);
        }
      }
      return expr;
    }

    IndexExpr einsum(const IndexExpr& expr) {
      onlyOneTerm = true;
      IndexExpr einsumexpr = rewrite(expr);

      if (onlyOneTerm) {
        einsumexpr = addReductions(einsumexpr);
      }

      return einsumexpr;
    }

    using ExprRewriter::visit;

    void visit(const AddNode* op) {
      // Sum every reduction variables over each term
      onlyOneTerm = false;

      IndexExpr a = addReductions(op->a);
      IndexExpr b = addReductions(op->b);
      if (a == op->a && b == op->b) {
        expr = op;
      }
      else {
        expr = new AddNode(a, b);
      }
    }

    void visit(const SubNode* op) {
      // Sum every reduction variables over each term
      onlyOneTerm = false;

      IndexExpr a = addReductions(op->a);
      IndexExpr b = addReductions(op->b);
      if (a == op->a && b == op->b) {
        expr = op;
      }
      else {
        expr = new SubNode(a, b);
      }
    }
  };

  return Einsum(free).einsum(expr);
}

IndexExpr einsum(const TensorVar& var) {
  return einsum(var.getIndexExpr(), var.getFreeVars());
}

}
