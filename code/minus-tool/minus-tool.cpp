// Clang includes
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Rewrite/Frontend/FixItRewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// LLVM includes
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

// Standard includes
#include <memory>
#include <string>
#include <type_traits>

namespace MinusTool {

class FixItRewriterOptions : public clang::FixItOptions {
 public:
  /// Constructor.
  ///
  /// The \p RewriteSuffix is the option from the command line.
  explicit FixItRewriterOptions(const std::string& RewriteSuffix)
  : RewriteSuffix(RewriteSuffix) {
    clang::FixItOptions::InPlace = false;
  }

  /// For a file to be rewritten, returns the (possibly) new filename.
  ///
  /// If the \c RewriteSuffix is empty, returns the \p Filename, causing
  /// in-place rewriting. If it is not empty, the \p Filename with that suffix
  /// is returned.
  std::string
  RewriteFilename(const std::string& Filename, int& FileDescriptor) override {
    // Don't need the file descriptor.
    FileDescriptor = -1;

    llvm::errs() << "Rewriting FixIts ";

    if (RewriteSuffix.empty()) {
      llvm::errs() << "in-place\n";
      return Filename;
    }

    const auto NewFilename = Filename + RewriteSuffix;
    llvm::errs() << "from " << Filename << " to " << NewFilename << "\n";

    return NewFilename;
  }

 private:
  /// The suffix appended to rewritten files.
  std::string RewriteSuffix;
};

class MatchHandler : public clang::ast_matchers::MatchFinder::MatchCallback {
 public:
  using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;
  using RewriterPointer = std::unique_ptr<clang::FixItRewriter>;

  /// Constructor.
  ///
  /// \p DoRewrite and \p RewriteSuffix are the command line options passed
  /// to the tool.
  MatchHandler(bool DoRewrite, const std::string& RewriteSuffix)
  : FixItOptions(RewriteSuffix), DoRewrite(DoRewrite) {}

  /// Runs the MatchHandler's action.
  ///
  /// Emits a diagnostic for each matched expression, optionally rewriting the
  /// file in-place or to another file, depending on the command line options.
  void run(const MatchResult& Result) {
    const auto& Op = Result.Nodes.getNodeAs<clang::BinaryOperator>("op");

    const auto StartLocation = Op->getOperatorLoc();
    const auto EndLocation = StartLocation.getLocWithOffset(+1);
    const clang::SourceRange SourceRange(StartLocation, EndLocation);
    const auto FixIt = clang::FixItHint::CreateReplacement(SourceRange, "-");

    auto& Context = *Result.Context;
    auto& DiagnosticsEngine = Context.getDiagnostics();

    // The FixItRewriter is quite a heavy object, so let's
    // not create it unless we really have to.
    RewriterPointer Rewriter;
    if (DoRewrite) {
      Rewriter = createRewriter(DiagnosticsEngine, Context);
    }

    const auto ID =
        DiagnosticsEngine.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                                          "This should probably be a minus");

    DiagnosticsEngine.Report(StartLocation, ID).AddFixItHint(FixIt);

    if (DoRewrite) {
      assert(Rewriter != nullptr);
      Rewriter->WriteFixedFiles();
    }
  }

 private:
  /// Allocates a \c FixItRewriter and sets it as the client of the given \p
  /// DiagnosticsEngine.
  ///
  /// The \p Context is forwarded to the constructor of the \c FixItRewriter.
  RewriterPointer createRewriter(clang::DiagnosticsEngine& DiagnosticsEngine,
                                 clang::ASTContext& Context) {
    auto Rewriter =
        std::make_unique<clang::FixItRewriter>(DiagnosticsEngine,
                                               Context.getSourceManager(),
                                               Context.getLangOpts(),
                                               &FixItOptions);

    DiagnosticsEngine.setClient(Rewriter.get(), /*ShouldOwnClient=*/false);

    return Rewriter;
  }

  FixItRewriterOptions FixItOptions;
  bool DoRewrite;
};

/// Consumes an AST and attempts to match for the
/// kinds of nodes we are looking for.
class Consumer : public clang::ASTConsumer {
 public:
  /// Constructor.
  ///
  /// All arguments are forwarded to the \c MatchHandler.
  template <typename... Args>
  explicit Consumer(Args&&... args) : Handler(std::forward<Args>(args)...) {
    using namespace clang::ast_matchers;

    // Want to match:
    // int x = 4   +   2;
    //     ^   ^   ^   ^
    //   var  lhs op  rhs

    // clang-format off
    const auto Matcher = varDecl(
       hasType(isInteger()),
       hasInitializer(binaryOperator(
         hasOperatorName("+"),
         hasLHS(integerLiteral().bind("lhs")),
         hasRHS(integerLiteral().bind("rhs"))).bind("op"))).bind("var");
    // clang-format on

    MatchFinder.addMatcher(Matcher, &Handler);
  }

  /// Attempts to match the match expression defined in the constructor.
  void HandleTranslationUnit(clang::ASTContext& Context) override {
    MatchFinder.matchAST(Context);
  }

 private:
  /// Our callback for matches.
  MatchHandler Handler;

  /// The MatchFinder we use for matching on the AST.
  clang::ast_matchers::MatchFinder MatchFinder;
};

class Action : public clang::ASTFrontendAction {
 public:
  using ASTConsumerPointer = std::unique_ptr<clang::ASTConsumer>;

  /// Constructor, taking the \p RewriteOption and \p RewriteSuffixOption.
  Action(bool DoRewrite, const std::string& RewriteSuffix)
  : DoRewrite(DoRewrite), RewriteSuffix(RewriteSuffix) {}

  /// Creates the Consumer instance, forwarding the command line options.
  ASTConsumerPointer CreateASTConsumer(clang::CompilerInstance& Compiler,
                                       llvm::StringRef Filename) override {
    return std::make_unique<Consumer>(DoRewrite, RewriteSuffix);
  }

 private:
  /// Whether we want to rewrite files. Forwarded to the consumer.
  bool DoRewrite;

  /// The suffix for rewritten files. Forwarded to the consumer.
  std::string RewriteSuffix;
};
}  // namespace MinusTool

namespace {
llvm::cl::OptionCategory ToolCategory("minus-tool options");

llvm::cl::extrahelp MinusToolCategoryHelp(R"(
This tool turns all your plusses into minuses, because why not.
Given a binary plus operation with two integer operands:

int x = 4 + 2;

This tool will rewrite the code to change the plus into a minus:

int x = 4 - 2;

You're welcome.
)");

llvm::cl::opt<bool>
    RewriteOption("rewrite",
                  llvm::cl::init(false),
                  llvm::cl::desc("If set, emits rewritten source code"),
                  llvm::cl::cat(ToolCategory));

llvm::cl::opt<std::string> RewriteSuffixOption(
    "rewrite-suffix",
    llvm::cl::desc("If -rewrite is set, changes will be rewritten to a file "
                   "with the same name, but this suffix"),
    llvm::cl::cat(ToolCategory));

llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
}  // namespace

/// A custom \c FrontendActionFactory so that we can pass the options
/// to the constructor of the tool.
struct ToolFactory : public clang::tooling::FrontendActionFactory {
  std::unique_ptr<clang::FrontendAction> create() {
    return std::make_unique<MinusTool::Action>(RewriteOption,
                                               RewriteSuffixOption);
  }
};


int main(int argc, const char** argv) {
  using namespace clang::tooling;

  auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }

  CommonOptionsParser& OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  return Tool.run(new ToolFactory());
}