// SleepCheckPlugin.cpp — Clang plugin enforcing sleep-safety annotations
//
// Rules:
//  1. Functions can be tagged nosleep, might_sleep, or sleeps.
//  2. Leaf functions (no calls) are implicitly nosleep.
//  3. Functions calling only nosleep functions are implicitly nosleep.
//  4. Untagged forward declarations are implicitly might_sleep.
//  5. Explicit tag on definition but not declaration (or vice versa) is an error.
//  6. Untagged function definition that is implicitly nosleep emits a warning.
//  7. nosleep function calling might_sleep or sleeps is an error.
//  8. Untagged function pointers are implicitly might_sleep.
//  9. Assigning a might_sleep/sleeps function to a nosleep function pointer is
//     an error.
// 10. Builtins/intrinsics are implicitly nosleep.
// 11. nosleep lexical blocks inside functions: calls within the block are
//     checked as if the function were nosleep. Nested blocks are allowed.
// 12. Jumping into a nosleep block from outside one is an error.

#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/Attr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/ParsedAttrInfo.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/Sema/ParsedAttr.h"
#include "clang/Sema/Sema.h"
#include "clang/Sema/SemaDiagnostic.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <string>
#include <vector>

using namespace clang;

// ---------------------------------------------------------------------------
// Sleep status enum
// ---------------------------------------------------------------------------
enum class SleepStatus { Unknown, NoSleep, MightSleep, Sleeps };

static const char *sleepStatusStr(SleepStatus s) {
  switch (s) {
  case SleepStatus::NoSleep:    return "nosleep";
  case SleepStatus::MightSleep: return "might_sleep";
  case SleepStatus::Sleeps:     return "sleeps";
  default:                      return "unknown";
  }
}

// ---------------------------------------------------------------------------
// Attribute registration — three GNU-style attrs that attach AnnotateAttr
// ---------------------------------------------------------------------------
static ParsedAttrInfo::AttrHandling
applySleepAnnotation(Sema &S, Decl *D, const ParsedAttr &Attr,
                     const char *annotation) {
  D->addAttr(AnnotateAttr::Create(S.Context, annotation, nullptr, 0,
                                  Attr.getRange()));
  return ParsedAttrInfo::AttributeApplied;
}

namespace {

struct NoSleepAttrInfo : public ParsedAttrInfo {
  static constexpr Spelling S[] = {
      {AttributeCommonInfo::AS_GNU, "nosleep"},
      {AttributeCommonInfo::AS_CXX11, "nosleep"},
  };
  NoSleepAttrInfo() {
    OptArgs = 0;
    NumArgs = 0;
    IsStmt = 1;
    Spellings = S;
  }
  bool diagAppertainsToDecl(Sema &S, const ParsedAttr &Attr,
                            const Decl *D) const override {
    if (!isa<FunctionDecl>(D) && !isa<VarDecl>(D) && !isa<TypedefNameDecl>(D) &&
        !isa<FieldDecl>(D)) {
      S.Diag(Attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
          << Attr << Attr.isRegularKeywordAttribute()
          << "functions, variables, typedefs, or fields";
      return false;
    }
    return true;
  }
  AttrHandling handleDeclAttribute(Sema &S, Decl *D,
                                   const ParsedAttr &Attr) const override {
    return applySleepAnnotation(S, D, Attr, "nosleep");
  }
  AttrHandling handleStmtAttribute(Sema &S, Stmt *St,
                                   const ParsedAttr &Attr,
                                   class Attr *&Result) const override {
    Result = AnnotateAttr::Create(S.Context, "nosleep", nullptr, 0,
                                  Attr.getRange());
    return AttributeApplied;
  }
};

struct MightSleepAttrInfo : public ParsedAttrInfo {
  static constexpr Spelling S[] = {
      {AttributeCommonInfo::AS_GNU, "might_sleep"},
      {AttributeCommonInfo::AS_CXX11, "might_sleep"},
  };
  MightSleepAttrInfo() {
    OptArgs = 0;
    NumArgs = 0;
    Spellings = S;
  }
  bool diagAppertainsToDecl(Sema &S, const ParsedAttr &Attr,
                            const Decl *D) const override {
    if (!isa<FunctionDecl>(D) && !isa<VarDecl>(D) && !isa<TypedefNameDecl>(D) &&
        !isa<FieldDecl>(D)) {
      S.Diag(Attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
          << Attr << Attr.isRegularKeywordAttribute()
          << "functions, variables, typedefs, or fields";
      return false;
    }
    return true;
  }
  AttrHandling handleDeclAttribute(Sema &S, Decl *D,
                                   const ParsedAttr &Attr) const override {
    return applySleepAnnotation(S, D, Attr, "might_sleep");
  }
};

struct SleepsAttrInfo : public ParsedAttrInfo {
  static constexpr Spelling S[] = {
      {AttributeCommonInfo::AS_GNU, "sleeps"},
      {AttributeCommonInfo::AS_CXX11, "sleeps"},
  };
  SleepsAttrInfo() {
    OptArgs = 0;
    NumArgs = 0;
    Spellings = S;
  }
  bool diagAppertainsToDecl(Sema &S, const ParsedAttr &Attr,
                            const Decl *D) const override {
    if (!isa<FunctionDecl>(D) && !isa<VarDecl>(D) && !isa<TypedefNameDecl>(D) &&
        !isa<FieldDecl>(D)) {
      S.Diag(Attr.getLoc(), diag::warn_attribute_wrong_decl_type_str)
          << Attr << Attr.isRegularKeywordAttribute()
          << "functions, variables, typedefs, or fields";
      return false;
    }
    return true;
  }
  AttrHandling handleDeclAttribute(Sema &S, Decl *D,
                                   const ParsedAttr &Attr) const override {
    return applySleepAnnotation(S, D, Attr, "sleeps");
  }
};

} // anonymous namespace

static ParsedAttrInfoRegistry::Add<NoSleepAttrInfo>
    XAttr1("nosleep", "marks function as nosleep");
static ParsedAttrInfoRegistry::Add<MightSleepAttrInfo>
    XAttr2("might_sleep", "marks function as might_sleep");
static ParsedAttrInfoRegistry::Add<SleepsAttrInfo>
    XAttr3("sleeps", "marks function as sleeps");

// ---------------------------------------------------------------------------
// Helpers: read sleep annotation from a Decl
// ---------------------------------------------------------------------------
// Get the sleep annotation written directly on this declaration (not inherited).
static SleepStatus getOwnAnnotation(const Decl *D) {
  for (const auto *A : D->specific_attrs<AnnotateAttr>()) {
    if (A->isInherited())
      continue;
    StringRef v = A->getAnnotation();
    if (v == "nosleep")     return SleepStatus::NoSleep;
    if (v == "might_sleep") return SleepStatus::MightSleep;
    if (v == "sleeps")      return SleepStatus::Sleeps;
  }
  return SleepStatus::Unknown;
}

// Get sleep annotation including inherited ones (effective annotation).
static SleepStatus getExplicitAnnotation(const Decl *D) {
  for (const auto *A : D->specific_attrs<AnnotateAttr>()) {
    StringRef v = A->getAnnotation();
    if (v == "nosleep")     return SleepStatus::NoSleep;
    if (v == "might_sleep") return SleepStatus::MightSleep;
    if (v == "sleeps")      return SleepStatus::Sleeps;
  }
  return SleepStatus::Unknown;
}

// Get sleep annotation from a variable's type (e.g. nosleep function pointer
// typedef).
static SleepStatus getTypeAnnotation(QualType QT) {
  while (true) {
    if (const auto *TDT = QT->getAs<TypedefType>()) {
      SleepStatus s = getExplicitAnnotation(TDT->getDecl());
      if (s != SleepStatus::Unknown)
        return s;
      QT = TDT->getDecl()->getUnderlyingType();
      continue;
    }
    if (const auto *PT = QT->getAs<PointerType>()) {
      QT = PT->getPointeeType();
      continue;
    }
    break;
  }
  return SleepStatus::Unknown;
}

// Get the effective annotation for a variable (explicit on var, or from type).
static SleepStatus getVarAnnotation(const VarDecl *VD) {
  SleepStatus s = getExplicitAnnotation(VD);
  if (s != SleepStatus::Unknown)
    return s;
  return getTypeAnnotation(VD->getType());
}

static SleepStatus getFieldAnnotation(const FieldDecl *FD) {
  SleepStatus s = getExplicitAnnotation(FD);
  if (s != SleepStatus::Unknown)
    return s;
  return getTypeAnnotation(FD->getType());
}

// Check if a VarDecl or FieldDecl holds a function pointer type
static bool isFuncPtrType(QualType QT) {
  QT = QT.getCanonicalType();
  if (const auto *PT = QT->getAs<PointerType>())
    return PT->getPointeeType()->isFunctionType();
  return false;
}

// ---------------------------------------------------------------------------
// Information we collect per function
// ---------------------------------------------------------------------------
struct FuncInfo {
  const FunctionDecl *Def = nullptr;
  SleepStatus ExplicitTag = SleepStatus::Unknown;
  SleepStatus Computed = SleepStatus::Unknown;
  bool HasBody = false;
  std::set<const FunctionDecl *> Callees;
  bool CallsThroughUntaggedFPtr = false;
};

// ---------------------------------------------------------------------------
// AST Consumer
// ---------------------------------------------------------------------------
namespace {

class SleepCheckConsumer : public ASTConsumer {
  CompilerInstance &CI;

  unsigned DiagTagMismatch;
  unsigned DiagSuggestNoSleep;
  unsigned DiagNoSleepCallsBad;
  unsigned DiagFPtrAssign;
  unsigned DiagNoSleepBlockCall;
  unsigned DiagNoSleepBlockFPtr;
  unsigned DiagGotoIntoNoSleep;

  std::map<const FunctionDecl *, FuncInfo> Funcs;

public:
  explicit SleepCheckConsumer(CompilerInstance &CI) : CI(CI) {
    auto &DE = CI.getDiagnostics();
    DiagTagMismatch = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "sleep annotation mismatch: declaration is '%0' but definition is '%1'");
    DiagSuggestNoSleep = DE.getCustomDiagID(
        DiagnosticsEngine::Warning,
        "function '%0' makes no sleeping calls; consider marking it "
        "'__attribute__((nosleep))'");
    DiagNoSleepCallsBad = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "'nosleep' function '%0' calls '%1' function '%2'");
    DiagFPtrAssign = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "assigning '%0' function '%1' to 'nosleep' function pointer");
    DiagNoSleepBlockCall = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "call to '%0' function '%1' inside 'nosleep' block");
    DiagNoSleepBlockFPtr = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "call through '%0' function pointer inside 'nosleep' block");
    DiagGotoIntoNoSleep = DE.getCustomDiagID(
        DiagnosticsEngine::Error,
        "goto jumps into 'nosleep' block from outside a 'nosleep' context");
  }

  void HandleTranslationUnit(ASTContext &Ctx) override;

private:
  void collectCallees(const Stmt *S, FuncInfo &Info, ASTContext &Ctx);
  void checkFPtrAssigns(const Stmt *S, ASTContext &Ctx,
                        DiagnosticsEngine &DE);
  void checkVarInit(const VarDecl *VD, DiagnosticsEngine &DE);
  SleepStatus resolveCallee(const FunctionDecl *Callee);
  void checkNoSleepBlocks(const FunctionDecl *FD, ASTContext &Ctx,
                          DiagnosticsEngine &DE);
  void walkNoSleepBlocks(const Stmt *S, unsigned Depth, ASTContext &Ctx,
                         DiagnosticsEngine &DE,
                         std::map<const LabelDecl *, unsigned> &LabelDepths,
                         std::vector<std::pair<const GotoStmt *, unsigned>> &Gotos);
};

// ---------------------------------------------------------------------------
// Collect direct callees and function-pointer calls from a function body
// ---------------------------------------------------------------------------
void SleepCheckConsumer::collectCallees(const Stmt *S, FuncInfo &Info,
                                        ASTContext &Ctx) {
  if (!S)
    return;

  if (const auto *CE = dyn_cast<CallExpr>(S)) {
    if (const FunctionDecl *Callee = CE->getDirectCallee()) {
      Info.Callees.insert(Callee->getCanonicalDecl());
    } else {
      // Indirect call — check annotation on the pointer
      const Expr *CalleeExpr = CE->getCallee()->IgnoreParenImpCasts();
      SleepStatus fpStatus = SleepStatus::Unknown;

      if (const auto *DRE = dyn_cast<DeclRefExpr>(CalleeExpr)) {
        if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl()))
          fpStatus = getVarAnnotation(VD);
      } else if (const auto *ME = dyn_cast<MemberExpr>(CalleeExpr)) {
        if (const auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl()))
          fpStatus = getFieldAnnotation(FD);
      }

      if (fpStatus == SleepStatus::Unknown) {
        // Rule 8: untagged function pointer → implicitly might_sleep
        Info.CallsThroughUntaggedFPtr = true;
      }
      // If tagged nosleep, fine. If tagged might_sleep/sleeps, the calling
      // function will inherit that via CallsThroughUntaggedFPtr being false
      // but we still need to flag nosleep callers. We'll handle that via a
      // separate check below — but to keep the fixed-point simple, treat
      // any non-nosleep fptr call like an untagged one for propagation:
      if (fpStatus == SleepStatus::MightSleep || fpStatus == SleepStatus::Sleeps)
        Info.CallsThroughUntaggedFPtr = true;
    }
  }

  for (const Stmt *Child : S->children())
    collectCallees(Child, Info, Ctx);
}

// ---------------------------------------------------------------------------
// Resolve the effective sleep status of a callee
// ---------------------------------------------------------------------------
SleepStatus SleepCheckConsumer::resolveCallee(const FunctionDecl *Callee) {
  auto it = Funcs.find(Callee);
  if (it != Funcs.end())
    return it->second.Computed;
  if (Callee->getBuiltinID() != 0)
    return SleepStatus::NoSleep;
  // Extern not in our map → might_sleep (rule 4)
  return SleepStatus::MightSleep;
}

// ---------------------------------------------------------------------------
// Check function-pointer assignments inside statements (rule 9)
// ---------------------------------------------------------------------------
void SleepCheckConsumer::checkFPtrAssigns(const Stmt *S, ASTContext &Ctx,
                                          DiagnosticsEngine &DE) {
  if (!S)
    return;

  // Binary assignment: fp = func
  if (const auto *BO = dyn_cast<BinaryOperator>(S)) {
    if (BO->getOpcode() == BO_Assign) {
      const Expr *LHS = BO->getLHS()->IgnoreParenImpCasts();
      const Expr *RHS = BO->getRHS()->IgnoreParenImpCasts();

      SleepStatus lhsStatus = SleepStatus::Unknown;
      if (const auto *DRE = dyn_cast<DeclRefExpr>(LHS)) {
        if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl())) {
          if (isFuncPtrType(VD->getType()))
            lhsStatus = getVarAnnotation(VD);
        }
      }

      if (lhsStatus == SleepStatus::NoSleep) {
        // Check what's being assigned
        if (const auto *DRHS = dyn_cast<DeclRefExpr>(RHS)) {
          if (const auto *FD = dyn_cast<FunctionDecl>(DRHS->getDecl())) {
            SleepStatus rhsStatus = resolveCallee(FD->getCanonicalDecl());
            if (rhsStatus == SleepStatus::MightSleep ||
                rhsStatus == SleepStatus::Sleeps) {
              DE.Report(BO->getOperatorLoc(), DiagFPtrAssign)
                  << sleepStatusStr(rhsStatus) << FD->getNameAsString();
            }
          }
        }
      }
    }
  }

  // Local variable declarations with initializer: safe_fp p = sleepy;
  if (const auto *DS = dyn_cast<DeclStmt>(S)) {
    for (const auto *D : DS->decls()) {
      if (const auto *VD = dyn_cast<VarDecl>(D))
        checkVarInit(VD, DE);
    }
  }

  for (const Stmt *Child : S->children())
    checkFPtrAssigns(Child, Ctx, DE);
}

// ---------------------------------------------------------------------------
// Check variable initializer for function-pointer assignment (rule 9)
// ---------------------------------------------------------------------------
void SleepCheckConsumer::checkVarInit(const VarDecl *VD,
                                      DiagnosticsEngine &DE) {
  if (!isFuncPtrType(VD->getType()))
    return;

  SleepStatus ptrStatus = getVarAnnotation(VD);
  if (ptrStatus != SleepStatus::NoSleep)
    return;

  const Expr *Init = VD->getInit();
  if (!Init)
    return;

  Init = Init->IgnoreParenImpCasts();
  if (const auto *DRE = dyn_cast<DeclRefExpr>(Init)) {
    if (const auto *FD = dyn_cast<FunctionDecl>(DRE->getDecl())) {
      SleepStatus rhsStatus = resolveCallee(FD->getCanonicalDecl());
      if (rhsStatus == SleepStatus::MightSleep ||
          rhsStatus == SleepStatus::Sleeps) {
        DE.Report(VD->getLocation(), DiagFPtrAssign)
            << sleepStatusStr(rhsStatus) << FD->getNameAsString();
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Helper: check if an AttributedStmt has a nosleep annotation
// ---------------------------------------------------------------------------
static bool isNoSleepBlock(const AttributedStmt *AS) {
  for (const auto *A : AS->getAttrs()) {
    if (const auto *Ann = dyn_cast<AnnotateAttr>(A)) {
      if (Ann->getAnnotation() == "nosleep")
        return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Walk function body checking calls inside nosleep blocks (rules 11, 12)
// ---------------------------------------------------------------------------
void SleepCheckConsumer::walkNoSleepBlocks(
    const Stmt *S, unsigned Depth, ASTContext &Ctx, DiagnosticsEngine &DE,
    std::map<const LabelDecl *, unsigned> &LabelDepths,
    std::vector<std::pair<const GotoStmt *, unsigned>> &Gotos) {
  if (!S)
    return;

  // Track nosleep block boundaries
  if (const auto *AS = dyn_cast<AttributedStmt>(S)) {
    if (isNoSleepBlock(AS)) {
      walkNoSleepBlocks(AS->getSubStmt(), Depth + 1, Ctx, DE,
                        LabelDepths, Gotos);
      return;
    }
  }

  // Record label depths for goto checking
  if (const auto *LS = dyn_cast<LabelStmt>(S)) {
    LabelDepths[LS->getDecl()] = Depth;
    // Continue walking the sub-statement of the label
    walkNoSleepBlocks(LS->getSubStmt(), Depth, Ctx, DE, LabelDepths, Gotos);
    return;
  }

  // Record gotos for later cross-checking
  if (const auto *GS = dyn_cast<GotoStmt>(S)) {
    Gotos.push_back({GS, Depth});
  }

  // Check calls inside nosleep blocks (Depth > 0 means we're in one)
  if (Depth > 0) {
    if (const auto *CE = dyn_cast<CallExpr>(S)) {
      if (const FunctionDecl *Callee = CE->getDirectCallee()) {
        SleepStatus cs = resolveCallee(Callee->getCanonicalDecl());
        if (cs == SleepStatus::MightSleep || cs == SleepStatus::Sleeps) {
          DE.Report(CE->getBeginLoc(), DiagNoSleepBlockCall)
              << sleepStatusStr(cs) << Callee->getNameAsString();
        }
      } else {
        // Indirect call through function pointer
        const Expr *CalleeExpr = CE->getCallee()->IgnoreParenImpCasts();
        SleepStatus fpStatus = SleepStatus::Unknown;

        if (const auto *DRE = dyn_cast<DeclRefExpr>(CalleeExpr)) {
          if (const auto *VD = dyn_cast<VarDecl>(DRE->getDecl()))
            fpStatus = getVarAnnotation(VD);
        } else if (const auto *ME = dyn_cast<MemberExpr>(CalleeExpr)) {
          if (const auto *FD = dyn_cast<FieldDecl>(ME->getMemberDecl()))
            fpStatus = getFieldAnnotation(FD);
        }

        if (fpStatus != SleepStatus::NoSleep) {
          const char *status = (fpStatus == SleepStatus::Unknown)
                                   ? "might_sleep"
                                   : sleepStatusStr(fpStatus);
          DE.Report(CE->getBeginLoc(), DiagNoSleepBlockFPtr) << status;
        }
      }
    }
  }

  for (const Stmt *Child : S->children())
    walkNoSleepBlocks(Child, Depth, Ctx, DE, LabelDepths, Gotos);
}

void SleepCheckConsumer::checkNoSleepBlocks(const FunctionDecl *FD,
                                            ASTContext &Ctx,
                                            DiagnosticsEngine &DE) {
  // For functions already tagged nosleep at function level, rule 7 handles
  // everything and nosleep blocks are redundant — skip block-level checking.
  const FunctionDecl *Canon = FD->getCanonicalDecl();
  auto it = Funcs.find(Canon);
  if (it != Funcs.end() && it->second.ExplicitTag == SleepStatus::NoSleep)
    return;

  std::map<const LabelDecl *, unsigned> LabelDepths;
  std::vector<std::pair<const GotoStmt *, unsigned>> Gotos;

  walkNoSleepBlocks(FD->getBody(), 0, Ctx, DE, LabelDepths, Gotos);

  // Rule 12: check gotos — error if jumping into nosleep from outside
  for (auto &[GS, GotoDepth] : Gotos) {
    const LabelDecl *Target = GS->getLabel();
    auto lit = LabelDepths.find(Target);
    if (lit == LabelDepths.end())
      continue; // label not found (shouldn't happen in valid code)
    unsigned LabelDep = lit->second;
    if (LabelDep > 0 && GotoDepth == 0) {
      DE.Report(GS->getGotoLoc(), DiagGotoIntoNoSleep);
    }
  }
}

// ---------------------------------------------------------------------------
// Main analysis entry point
// ---------------------------------------------------------------------------
void SleepCheckConsumer::HandleTranslationUnit(ASTContext &Ctx) {
  auto &DE = CI.getDiagnostics();

  // ---- Phase 1: Collect all FunctionDecls, record explicit annotations ----
  for (auto *D : Ctx.getTranslationUnitDecl()->decls()) {
    auto *FD = dyn_cast<FunctionDecl>(D);
    if (!FD)
      continue;

    const FunctionDecl *Canon = FD->getCanonicalDecl();

    if (Funcs.find(Canon) == Funcs.end()) {
      FuncInfo &FI = Funcs[Canon];
      FI.ExplicitTag = getOwnAnnotation(Canon);
    }

    FuncInfo &FI = Funcs[Canon];

    if (FD->doesThisDeclarationHaveABody()) {
      FI.HasBody = true;
      FI.Def = FD;
      collectCallees(FD->getBody(), FI, Ctx);

      SleepStatus defTag = getOwnAnnotation(FD);

      // Rule 5: check consistency between declaration and definition
      SleepStatus declTag = SleepStatus::Unknown;
      for (auto *RD : FD->redecls()) {
        if (RD == FD)
          continue;
        SleepStatus rt = getOwnAnnotation(RD);
        if (rt != SleepStatus::Unknown) {
          declTag = rt;
          break;
        }
      }

      if (defTag != SleepStatus::Unknown) {
        if (declTag != SleepStatus::Unknown && declTag != defTag) {
          DE.Report(FD->getLocation(), DiagTagMismatch)
              << sleepStatusStr(declTag) << sleepStatusStr(defTag);
        }
        FI.ExplicitTag = defTag;
      } else if (declTag != SleepStatus::Unknown) {
        DE.Report(FD->getLocation(), DiagTagMismatch)
            << sleepStatusStr(declTag) << "untagged";
      }
    }
  }

  // ---- Phase 2: Fixed-point computation of sleep status ----
  // Initialize
  for (auto &[Canon, FI] : Funcs) {
    if (FI.ExplicitTag != SleepStatus::Unknown) {
      FI.Computed = FI.ExplicitTag;
      continue;
    }
    if (Canon->getBuiltinID() != 0) {
      FI.Computed = SleepStatus::NoSleep; // rule 10
      continue;
    }
    if (FI.HasBody) {
      if (FI.Callees.empty() && !FI.CallsThroughUntaggedFPtr) {
        FI.Computed = SleepStatus::NoSleep; // rule 2
      }
      // else Unknown — resolved in iteration
    } else {
      FI.Computed = SleepStatus::MightSleep; // rule 4
    }
  }

  // Iterate until stable
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &[Canon, FI] : Funcs) {
      if (FI.Computed != SleepStatus::Unknown)
        continue;
      if (!FI.HasBody)
        continue;

      if (FI.CallsThroughUntaggedFPtr) {
        FI.Computed = SleepStatus::MightSleep;
        changed = true;
        continue;
      }

      bool allResolved = true;
      bool allNoSleep = true;
      for (const auto *Callee : FI.Callees) {
        SleepStatus cs = resolveCallee(Callee);
        if (cs == SleepStatus::Unknown) {
          allResolved = false;
          break;
        }
        if (cs != SleepStatus::NoSleep)
          allNoSleep = false;
      }

      if (!allResolved)
        continue;

      FI.Computed = allNoSleep ? SleepStatus::NoSleep   // rule 3
                               : SleepStatus::MightSleep;
      changed = true;
    }
  }

  // Remaining Unknown (cycles) → might_sleep
  for (auto &[Canon, FI] : Funcs) {
    if (FI.Computed == SleepStatus::Unknown)
      FI.Computed = SleepStatus::MightSleep;
  }

  // ---- Phase 3: Emit diagnostics ----

  for (auto &[Canon, FI] : Funcs) {
    if (!FI.HasBody || !FI.Def)
      continue;

    // Rule 6: untagged definition that turned out nosleep
    if (FI.ExplicitTag == SleepStatus::Unknown &&
        FI.Computed == SleepStatus::NoSleep) {
      DE.Report(FI.Def->getLocation(), DiagSuggestNoSleep)
          << FI.Def->getNameAsString();
    }

    // Rule 7: explicitly nosleep function calling might_sleep/sleeps
    if (FI.ExplicitTag == SleepStatus::NoSleep) {
      for (const auto *Callee : FI.Callees) {
        SleepStatus cs = resolveCallee(Callee);
        if (cs == SleepStatus::MightSleep || cs == SleepStatus::Sleeps) {
          DE.Report(FI.Def->getLocation(), DiagNoSleepCallsBad)
              << FI.Def->getNameAsString() << sleepStatusStr(cs)
              << Callee->getNameAsString();
        }
      }
      if (FI.CallsThroughUntaggedFPtr) {
        DE.Report(FI.Def->getLocation(), DiagNoSleepCallsBad)
            << FI.Def->getNameAsString() << "might_sleep"
            << "(function pointer)";
      }
    }
  }

  // Rule 9: function-pointer assignments
  // Rules 11, 12: nosleep blocks and goto checking
  for (auto *D : Ctx.getTranslationUnitDecl()->decls()) {
    if (auto *FD = dyn_cast<FunctionDecl>(D)) {
      if (FD->doesThisDeclarationHaveABody()) {
        checkFPtrAssigns(FD->getBody(), Ctx, DE);
        checkNoSleepBlocks(FD, Ctx, DE);
      }
    }
    if (auto *VD = dyn_cast<VarDecl>(D))
      checkVarInit(VD, DE);
  }
}

// ---------------------------------------------------------------------------
// Plugin action
// ---------------------------------------------------------------------------
class SleepCheckAction : public PluginASTAction {
public:
  std::unique_ptr<ASTConsumer>
  CreateASTConsumer(CompilerInstance &CI, llvm::StringRef) override {
    return std::make_unique<SleepCheckConsumer>(CI);
  }

  bool ParseArgs(const CompilerInstance &CI,
                 const std::vector<std::string> &args) override {
    return true;
  }

  ActionType getActionType() override { return AddAfterMainAction; }
};

} // anonymous namespace

static FrontendPluginRegistry::Add<SleepCheckAction>
    XPlugin("sleep-check", "Check sleep annotations on functions");
