//===--- SymbolExportCheck.cpp - clang-tidy -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SymbolExportCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "../../../clang/lib/Lex/UnicodeCharSets.h"
#include <iostream>
#include <fstream>

using namespace clang::ast_matchers;

namespace clang {
namespace tidy {
namespace performance {

SymbolExportCheck::SymbolExportCheck(StringRef Name, ClangTidyContext *Context)
: ClangTidyCheck(Name, Context) {
  exportMacro = Options.get("exportMacro", "");
  exportHdr = Options.get("exportHdr", "");
}

void SymbolExportCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "exportMacro", exportMacro);
  Options.store(Opts, "exportHdr", exportHdr);
}

void SymbolExportCheck::registerMatchers(MatchFinder *Finder) {
  auto isllvm = hasAncestor(namespaceDecl(hasName("llvm")));
  auto isnllvm = unless(isllvm);
  auto nosys = unless(isExpansionInSystemHeader());
  auto extsym = hasExternalFormalLinkage();
  auto hasM = hasDescendant(cxxMethodDecl(extsym).bind("method"));
  auto hasD = has(varDecl(extsym).bind("data"));
  // unless(isImplicit())
//  Finder->addMatcher(anyOf(cxxRecordDecl(nosys, hasD).bind("class"), varDecl(nosys, hasExternalFormalLinkage(), hasGlobalStorage()).bind("global")), this);
  Finder->addMatcher(cxxRecordDecl(nosys, hasD).bind("class"), this);
  Finder->addMatcher(varDecl(nosys, extsym, hasGlobalStorage()).bind("global"), this);
  // anyOf(hasM, hasD)
}

void SymbolExportCheck::check(const MatchFinder::MatchResult &Result) {
//  for (auto&& n : Result.Nodes.getMap())
 //   llvm::outs() << n.first << ' '  << n.second.getNodeKind() << '\n';

 // const auto *MatchedDecl = Result.Nodes.getNodeAs<FunctionDecl>("method");
	//__debugbreak();
  const auto *cd = Result.Nodes.getNodeAs<CXXRecordDecl>("class");
  const DeclaratorDecl* foundSomething = nullptr;
  if (cd)
  {
    
  for (auto&& ds : cd->decls())
    if (auto decl = dyn_cast<VarDecl>(ds)) // others are FieldDecls
    {
      if (!decl->isStaticDataMember())
	      continue;
      GVALinkage lg = Result.Context->GetGVALinkageForVariable(decl);
     if (lg != GVA_StrongExternal)
        continue;

      if (decl->hasDefinition())
          continue;
      foundSomething = decl;
      decl->printQualifiedName(llvm::outs());
      auto lv = decl->getLinkageAndVisibility();
      Linkage l = decl->getLinkageInternal();
      llvm::outs() << ' ' << lv.getLinkage() << ' ' << lv.getVisibility() << ' ' << l << ' ' << lg << '\n';
      diag(decl->getLocation(), "data %0 needs to be imported")
	      << decl
	      << FixItHint::CreateInsertion(decl->getLocation(), exportMacro + ' ');

//  break;
    }

  if (0) for (auto&& decl : cd->methods())
  {
    GVALinkage lg = Result.Context->GetGVALinkageForFunction(decl);
    if (lg != GVA_StrongExternal)
        continue;
    if (decl->isDefined())
        continue;
   foundSomething = decl;
    decl->printQualifiedName(llvm::outs());
    auto lv = decl->getLinkageAndVisibility();
    Linkage l = decl->getLinkageInternal();
    llvm::outs() << lv.getLinkage() << ' ' << lv.getVisibility() << ' ' << l << ' ' << lg << '\n';
	break;
  }

   }

  const auto *global = Result.Nodes.getNodeAs<VarDecl>("global");
  while (global)
  {
    if (global->isStaticDataMember())
      break;
    GVALinkage lg = Result.Context->GetGVALinkageForVariable(global);
    if (lg != GVA_StrongExternal)
      break;

    if (global->hasDefinition())
      break;

	foundSomething = global;
    diag(global->getLocation(), "data %0 needs to be imported")
      << global
      << FixItHint::CreateInsertion(global->getLocation(), exportMacro + ' ');

	break;
  }
  
  if (!foundSomething)
	  return;
  
  SourceLocation fileLoc = cd ? cd->getLocation() : global->getLocation();
#if 0
  StringRef filename = Result.SourceManager->getFilename(fileLoc);
  std::ifstream fileInput;
  std::string line;
  const char* search = "#include"; // test variable to search in file
  // open file to search
  fileInput.open((const char*)filename.bytes_begin());
  if (!fileInput.is_open())
	  return;

  unsigned int curLine = 0;
  while (std::getline(fileInput, line))
  {
	  curLine++;
	  if (line.find(search, 0) == 0) {
	    std::cout << "found: " << search << "line: " << curLine << std::endl;
		break;
	  }
  }
  fileInput.close();

#if 0
  diag(cd->getLocation(), "class %0 needs to be exported because of member %1")
	  << cd << foundSomething
      << FixItHint::CreateInsertion(cd->getLocation(), exportMacro + ' ');
#endif

  SourceLocation firstIncludeLoc = Result.SourceManager->translateLineCol(Result.SourceManager->getFileID(fileLoc), curLine, 1);
#else
  SourceLocation firstIncludeLoc = Result.SourceManager->translateLineCol(Result.SourceManager->getFileID(fileLoc), 1, 1);
#endif
  diag(firstIncludeLoc, "insert export include")
	  << FixItHint::CreateInsertion(firstIncludeLoc, "#include <" + exportHdr + ">\n");
}

} // namespace performance
} // namespace tidy
} // namespace clang
