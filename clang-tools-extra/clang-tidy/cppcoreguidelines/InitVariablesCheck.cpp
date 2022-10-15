//===--- InitVariablesCheck.cpp - clang-tidy ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "InitVariablesCheck.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/ParentMap.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace cppcoreguidelines {

namespace {
AST_MATCHER(VarDecl, isLocalVarDecl) { return Node.isLocalVarDecl(); }
} // namespace

InitVariablesCheck::InitVariablesCheck(StringRef Name,
                                       ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      IncludeInserter(Options.getLocalOrGlobal("IncludeStyle",
                                               utils::IncludeSorter::IS_LLVM),
                      areDiagsSelfContained()),
      MathHeader(Options.get("MathHeader", "<math.h>")) {}

void InitVariablesCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "IncludeStyle", IncludeInserter.getStyle());
  Options.store(Opts, "MathHeader", MathHeader);
}

void InitVariablesCheck::registerMatchers(MatchFinder *Finder) {
  auto localUndefinedVar =
      varDecl(unless(hasInitializer(anything())), unless(isInstantiated()),
              hasLocalStorage(), unless(parmVarDecl()), isDefinition(),
              hasAncestor(compoundStmt().bind("varscope")))
          .bind("varDecl");
  auto declRefToUninitVar =
      declRefExpr(hasDeclaration(localUndefinedVar),
                  hasAncestor(compoundStmt().bind("refscope")))
          .bind("ref");
  Finder->addMatcher(declRefToUninitVar, this);
#if 0
  declRefExpr(hasDeclaration(
      varDecl(GlobalVarDecl, unless(isDefinition())).bind("referencee")));
  Finder->addMatcher(
                         .bind("vardecl"),
                     this);
#endif
}

void InitVariablesCheck::registerPPCallbacks(const SourceManager &SM,
                                             Preprocessor *PP,
                                             Preprocessor *ModuleExpanderPP) {
  IncludeInserter.registerPreprocessor(PP);
}

static SourceLocation findSemiAfterLocation(SourceLocation loc, const ASTContext &Ctx,
                                            bool IsDecl) {
  const SourceManager &SM = Ctx.getSourceManager();
  if (loc.isMacroID()) {
    if (!Lexer::isAtEndOfMacroExpansion(loc, SM, Ctx.getLangOpts(), &loc))
      return SourceLocation();
  }
  loc = Lexer::getLocForEndOfToken(loc, /*Offset=*/0, SM, Ctx.getLangOpts());

  // Break down the source location.
  std::pair<FileID, unsigned> locInfo = SM.getDecomposedLoc(loc);

  // Try to load the file buffer.
  bool invalidTemp = false;
  StringRef file = SM.getBufferData(locInfo.first, &invalidTemp);
  if (invalidTemp)
    return SourceLocation();

  const char *tokenBegin = file.data() + locInfo.second;

  // Lex from the start of the given location.
  Lexer lexer(SM.getLocForStartOfFile(locInfo.first), Ctx.getLangOpts(),
              file.begin(), tokenBegin, file.end());
  Token tok;
  lexer.LexFromRawLexer(tok);
  if (tok.isNot(tok::semi)) {
    if (!IsDecl)
      return SourceLocation();
    // Declaration may be followed with other tokens; such as an __attribute,
    // before ending with a semicolon.
    return findSemiAfterLocation(tok.getLocation(), Ctx, /*IsDecl*/ true);
  }

  return tok.getLocation();
}
static SourceLocation findLocationAfterSemi(SourceLocation loc,
                                            const ASTContext &Ctx,
                                            bool IsDecl) {
  SourceLocation SemiLoc = findSemiAfterLocation(loc, Ctx, IsDecl);
  if (SemiLoc.isInvalid())
    return SourceLocation();
  return SemiLoc.getLocWithOffset(1);
}

void InitVariablesCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *varDecl = Result.Nodes.getNodeAs<VarDecl>("varDecl");
  const auto *refExpr = Result.Nodes.getNodeAs<DeclRefExpr>("ref");
  auto *varScope = Result.Nodes.getNodeAs<CompoundStmt>("varscope");
  const auto *refScope = Result.Nodes.getNodeAs<CompoundStmt>("refscope");
  const ASTContext &Context = *Result.Context;
  const SourceManager &Source = Context.getSourceManager();

  // We want to warn about cases where the type name
  // comes from a macro like this:
  //
  // TYPENAME_FROM_MACRO var;
  //
  // but not if the entire declaration comes from
  // one:
  //
  // DEFINE_SOME_VARIABLE();
  //
  // or if the definition comes from a macro like SWAP
  // that uses an internal temporary variable.
  //
  // Thus check that the variable name does
  // not come from a macro expansion.
  if (varDecl->getEndLoc().isMacroID())
    return;
  // varDecl->isThisDeclarationReferenced

  QualType TypePtr = varDecl->getType();
  llvm::Optional<const char *> InitializationString;
  bool AddMathInclude = false;
  clang::ParentMap parentMap(const_cast<CompoundStmt *>(varScope));
  const Stmt *v = refExpr;
  const Stmt *res = nullptr;
  uint32_t depth = 0;
  /*
int test2(int x)
{
    int i;

    if (x)
        i = 1;
    else
        i = 2;
    return i;
}*/
  while (true) {
    const Stmt *p = parentMap.getParent(v);
    if (auto compound = dyn_cast<CompoundStmt>(p)) {
      ++depth;
      if (compound == varScope) {
        res = v;
        //res->dump();
        break;
      }
    }
    v = p;
  }
//  auto range = CharSourceRange::getTokenRange(varDecl->getSourceRange());
  SourceLocation endLoc =
      findLocationAfterSemi(varDecl->getInnerLocStart(), Context, true);
  auto range = CharSourceRange::getTokenRange(varDecl->getInnerLocStart(), endLoc);

  diag(varDecl->getLocation(), "variable %0 can be moved")
      << varDecl
      << FixItHint::CreateInsertionFromRange(
             res->getBeginLoc(),
             // varDecl->getLocation().getLocWithOffset(varDecl->getName().size()),
             range)
      << FixItHint::CreateRemoval(range);

  if (TypePtr->isEnumeralType())
    InitializationString = nullptr;
  else if (TypePtr->isBooleanType())
    InitializationString = " = false";
  else if (TypePtr->isIntegerType())
    InitializationString = " = 0";
  else if (TypePtr->isFloatingType()) {
    InitializationString = " = NAN";
    AddMathInclude = true;
  } else if (TypePtr->isAnyPointerType()) {
    if (getLangOpts().CPlusPlus11)
      InitializationString = " = nullptr";
    else
      InitializationString = " = NULL";
  }

  if (false) {
    auto Diagnostic =
        diag(varDecl->getLocation(), "variable %0 is not initialized")
        << varDecl
        << FixItHint::CreateInsertion(varDecl->getLocation().getLocWithOffset(
                                          varDecl->getName().size()),
                                      InitializationString);
    if (AddMathInclude) {
      Diagnostic << IncludeInserter->CreateIncludeInsertion(
          Source.getFileID(varDecl->getBeginLoc()), MathHeader, false);
    }
  }
}
} // namespace cppcoreguidelines
} // namespace tidy
} // namespace clang
