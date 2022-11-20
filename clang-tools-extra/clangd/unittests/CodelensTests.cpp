//===-- XRefsTests.cpp  ---------------------------*- C++ -*--------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#include "Annotations.h"
#include "CodeLens.h"
#include "ParsedAST.h"
#include "Protocol.h"
#include "TestFS.h"
#include "TestTU.h"
#include "XRefs.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <string>
#include <vector>

namespace clang {
namespace clangd {
namespace {

using ::testing::ElementsAre;
using ::testing::Matcher;

MATCHER_P2(FileRange, File, Range, "") {
  return Location{URIForFile::canonicalize(File, testRoot()), Range} == arg;
}

CodeLens lens(const URIForFile &URI, const Range &Range) {
  return CodeLens{Range, std::nullopt, CodeLensResolveData{URI.file().str()}};
}

CodeLens lens(const URIForFile &URI, const std::string &Title, const Range &Range) {
  Command Cmd;
  Cmd.title = Title;
  Cmd.command = CodeAction::SHOW_REFERENCES;
  Cmd.argument = CodeLensResolveData{URI.file().str()};
  return CodeLens{Range, Cmd, std::nullopt};
}

TEST(CodeLensTest, Reference) {
  Annotations Main(R"cpp(
    class $X[[X]] {
    };
    int $main[[main]]() {
      X x;
    }
  )cpp");
  TestTU TU;
  TU.Code = std::string(Main.code());
  auto AST = TU.build();
  auto Path = URIForFile::canonicalize(testPath(TU.Filename), testRoot());
  if (auto CodeLens1 =
          getDocumentCodeLens(AST, nullptr, 0, testPath(TU.Filename))) {
    EXPECT_THAT(
        *CodeLens1,
        ElementsAre(lens(Path, Main.range("X")),
                    lens(Path, Main.range("main"))));
    std::vector<CodeLens> CodeLens2;
    for (auto &&CD : *CodeLens1) {
      if (auto CL = resolveCodeLens(AST, CD, 0, nullptr, testPath(TU.Filename)))
        CodeLens2.push_back(*CL);
    }
    EXPECT_THAT(
        CodeLens2,
        ElementsAre(
            lens(Path, "1 ref(s)", Main.range("X")),
            lens(Path, "0 ref(s)", Main.range("main"))));
  }
}

TEST(CodeLensTest, Inheritance) {
  Annotations Main(R"cpp(
    class $X[[X]] {
    public:
      virtual void $m1[[method]]();
    };

    class $Y[[Y]] : public X {
    public:
      void $m2[[method]]() override;
    };
  )cpp");
  TestTU TU;
  TU.Code = std::string(Main.code());
  auto AST = TU.build();
  auto URI = URIForFile::canonicalize(testPath(TU.Filename), testRoot());
  if (auto CodeLens = getDocumentCodeLens(AST, TU.index().get(), 0,
                                          testPath(TU.Filename))) {
	for (auto &&CD : *CodeLens) {
      llvm::outs() << "\nCodeLens: " << CD.range;
      if (CD.command)
        llvm::outs() << CD.command->title << CD.command->command << CD.command->argument;
      if (CD.data)
        llvm::outs() << "Uri: " << CD.data->uri;
	}
  {
    auto CD = lens(URI, "1 derived", Main.range("X"));
    llvm::outs() << "\nCodeLens: " << CD.range;
    if (CD.command)
        llvm::outs() << CD.command->title << CD.command->command
                     << CD.command->argument;
    if (CD.data)
        llvm::outs() << CD.data->uri;
  }
    EXPECT_THAT(*CodeLens, ElementsAre(lens(URI, Main.range("X")),
                                       lens(URI, "1 derived", Main.range("X")),
                                       lens(URI, Main.range("m1")),
                                       lens(URI, "1 derived", Main.range("m1")),
                                       lens(URI, Main.range("Y")),
                                       lens(URI, Main.range("m2"))
                                       // lens(URI, Main.range("Y")),
                                       // lens(URI, "1 base", Main.range("Y")),
                                       // lens(URI, Main.range("m2")),
                                       // lens(URI, "1 base", Main.range("m2"))
                                       ));
  }
}

} // namespace
} // namespace clangd
} // namespace clang
