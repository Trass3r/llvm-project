//===--- MacroUsageCheck.cpp - clang-tidy----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MacroUsageCheck.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Lex/Preprocessor.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Regex.h"
#include <algorithm>
#include <cctype>

namespace clang {
namespace tidy {
namespace cppcoreguidelines {

namespace {

bool isCapsOnly(StringRef Name) {
  return std::all_of(Name.begin(), Name.end(), [](const char c) {
    if (std::isupper(c) || std::isdigit(c) || c == '_')
      return true;
    return false;
  });
}

StringRef enumPrefix(StringRef Name) {
  size_t i = Name.find('_');
  if (i == StringRef::npos)
    return StringRef();
  return Name.substr(0, i);
}

class MacroUsageCallbacks final : public PPCallbacks {
public:
  MacroUsageCallbacks(MacroUsageCheck *Check, const SourceManager &SM,
                      StringRef RegExp, bool CapsOnly, bool IgnoreCommandLine)
      : Check(Check), SM(SM), RegExp(RegExp), CheckCapsOnly(CapsOnly),
        IgnoreCommandLineMacros(IgnoreCommandLine) {}
  void MacroDefined(const Token &MacroNameTok,
                    const MacroDirective *MD) override {
    if (SM.isWrittenInBuiltinFile(MD->getLocation()) ||
        MD->getMacroInfo()->isUsedForHeaderGuard() ||
        MD->getMacroInfo()->getNumTokens() == 0)
      return;

    if (IgnoreCommandLineMacros &&
        SM.isWrittenInCommandLineFile(MD->getLocation()))
      return;

    StringRef MacroName = MacroNameTok.getIdentifierInfo()->getName();
    if (!CheckCapsOnly && !llvm::Regex(RegExp).match(MacroName))
      Check->warnMacro(MD, MacroName, SM);

    if (CheckCapsOnly && !isCapsOnly(MacroName))
      Check->warnNaming(MD, MacroName);
  }

  void EndOfMainFile() override {
	  // TODO:
  }

private:
  MacroUsageCheck *Check;
  const SourceManager &SM;
  StringRef RegExp;
  bool CheckCapsOnly;
  bool IgnoreCommandLineMacros;
};
} // namespace

void MacroUsageCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "AllowedRegexp", AllowedRegexp);
  Options.store(Opts, "CheckCapsOnly", CheckCapsOnly);
  Options.store(Opts, "IgnoreCommandLineMacros", IgnoreCommandLineMacros);
}

void MacroUsageCheck::registerPPCallbacks(const SourceManager &SM,
                                          Preprocessor *PP,
                                          Preprocessor *ModuleExpanderPP) {
  PP->addPPCallbacks(std::make_unique<MacroUsageCallbacks>(
      this, SM, AllowedRegexp, CheckCapsOnly, IgnoreCommandLineMacros));
}

void MacroUsageCheck::warnMacro(const MacroDirective *MD, StringRef MacroName,
                                const SourceManager &SM) {
  StringRef Message =
      "macro '%0' used to declare a constant; consider using a 'constexpr' "
      "constant";

  auto MI = MD->getMacroInfo();
  /// A variadic macro is function-like at the same time. Therefore variadic
  /// macros are checked first and will be excluded for the function-like
  /// diagnostic.
  if (MI->isVariadic())
    Message = "variadic macro '%0' used; consider using a 'constexpr' "
              "variadic template function";
  else if (MI->isFunctionLike())
    Message = "function-like macro '%0' used; consider a 'constexpr' template "
              "function";

  if (!isCapsOnly(MacroName))
    return;
  StringRef prefix = enumPrefix(MacroName);
//  SM.getDecomposedLoc()
//  SM.translateLineCol(SM.getFileID(MD->getLocation()),
//      SM.getLineNumber(SM.getFileID(MD->getLocation()), SM.file MD->getLocation().), 1);
  if (!prefix.empty()) {
    if (prefix == enumPrefix(LastMacroPrefix.first)) {
      if (!InsideEnum) {
        InsideEnum = true;
        // TODO: emit start of enum before LastMacro
        // TODO: emit replacement for last macropre
        auto LastMD = LastMacroPrefix.second;
        auto LastMI = LastMD->getMacroInfo();
        diag(LastMD->getLocation(), "%0 could be first enum")
            << MacroName
            << FixItHint::CreateReplacement(
                   SourceRange(LastMD->getLocation().getLocWithOffset(-8),
                               LastMD->getLocation().getLocWithOffset(-1)),
                   ("enum " + prefix + " {\n\t").str())
            << FixItHint::CreateInsertion(LastMI->tokens_begin()->getLocation(),
                                          "= ")
            << FixItHint::CreateInsertion(
                   (LastMI->tokens_end() - 1)->getEndLoc(),
                                          ",");
      }
      SourceRange defSrcRange(MI->getDefinitionLoc(),
                              MI->getDefinitionEndLoc());
      SourceRange replacementSrcRange(MI->tokens_begin()->getLocation(),
                                      (MI->tokens_end() - 1)->getLocation());

	  diag(MD->getLocation(), "macro %0 could be enum")
          << MacroName
		  << FixItHint::CreateReplacement(SourceRange(
                   MD->getLocation().getLocWithOffset(-8),
			  MD->getLocation().getLocWithOffset(-1)), "\t")
          << FixItHint::CreateInsertion(MI->tokens_begin()->getLocation(),
                                        "= ")
          << FixItHint::CreateInsertion((MI->tokens_end() - 1)->getEndLoc(),
                                        ",", true);
    } else {
      if (InsideEnum) {
       // TODO: emit end
        diag((LastMacroPrefix.second->getMacroInfo()->tokens_end() - 1)->getEndLoc(), "macro end")
           << FixItHint::CreateInsertion(
                  (LastMacroPrefix.second->getMacroInfo()->tokens_end() - 1)
                      ->getEndLoc(),
                                          "\n};");

        InsideEnum = false;
	  }
    }
  }
  LastMacroPrefix.first = MacroName;
  LastMacroPrefix.second = MD;

  // diag(MD->getLocation(), Message) << MacroName;
}

void MacroUsageCheck::warnNaming(const MacroDirective *MD,
                                 StringRef MacroName) {
  diag(MD->getLocation(), "macro definition does not define the macro name "
                          "'%0' using all uppercase characters")
      << MacroName;
}

} // namespace cppcoreguidelines
} // namespace tidy
} // namespace clang
