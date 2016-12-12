//
// Rewrite all the names of names of variables and functions declared
// in an input source to be as short as possible.
//
// Usage:
//
//     ./rewriter foo.cl \
//         -extra-arg=-Dcl_clang_storage_class_specifiers \
//         -extra-arg=-I/Users/cec/phd/extern/libclc/generic/include \
//         -extra-arg=-include \
//         -extra-arg=/Users/cec/phd/extern/libclc/generic/include/clc/clc.h \
//         -extra-arg=-target -extra-arg=nvptx64-nvidia-nvcl \
//         -extra-arg=-xcl --
//
// TODO:
//
//   Read from stdin.
//   Rewrite user defined types.
//   Rewrite typedefs.
//
#include <memory>
#include <string>
#include <map>

#include <clang/AST/AST.h>
#include <clang/AST/ASTConsumer.h>
#include <clang/AST/ASTContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Driver/Options.h>
#include <clang/Frontend/ASTConsumers.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

// Error code which is returned if there was nothing to rewrite:
#define EUGLY_CODE 204

namespace rewriter {

static clang::Rewriter rewriter;
static llvm::cl::OptionCategory _tool_category("phd");

static unsigned int _fn_decl_rewrites_counter = 0;
static unsigned int _fn_call_rewrites_counter = 0;

static unsigned int _var_decl_rewrites_counter = 0;
static unsigned int _var_use_rewrites_counter = 0;

static bool isRewritten() {
  return (rewriter::_fn_decl_rewrites_counter ||
          rewriter::_fn_call_rewrites_counter ||
          rewriter::_var_decl_rewrites_counter ||
          rewriter::_var_use_rewrites_counter);
}

enum ctype { AZ, az };

// Increment a single character name. E.G. 'A' -> 'B', 'B' -> 'C',
// 'az' -> 'ba'.
//
std::string get_next_name(const std::string& current) {
  auto c = current;
  char *cc = &c[c.length() - 1];
  ctype type;

  while (true) {
    // determine type
    if (*cc >= 'A' && *cc <= 'Z')
      type = ctype::AZ;
    else
      type = ctype::az;

    ++*cc;

    // If the value has overflowed, reset and move to next char.
    if (*cc == 'Z' + 1 || *cc == 'z' + 1) {
      // reset char
      if (type == ctype::AZ)
        *cc = 'A';
      else
        *cc = 'a';


      // If we're at the last character, insert a new one, otherwise
      // just move to the next character to increment.
      if (cc == &c[0]) {
        if (type == ctype::AZ)
          c.insert(c.begin(), 'A');
        else
          c.insert(c.begin(), 'a');
        break;
      } else {
        --cc;
      }
    } else {
      break;
    }
  }

  return c;
}


class RewriterVisitor : public clang::RecursiveASTVisitor<RewriterVisitor> {
 private:
  std::unique_ptr<clang::ASTContext> _context;  // additional AST info

  // Function name rewriting:
  std::map<std::string, std::string> _fns;
  std::string _last_fn;

  // Variable name rewriting:
  std::map<std::string, std::string> _vars;
  std::string _last_var;

  // Accepts a function name, and returns the rewritten name.
  //
  std::string get_fn_rewrite(const std::string& name) {
    if (_fns.empty()) {
      // First function, seed the rewrite process:
      const auto seed = "A";
      _last_fn = seed;
      _fns[name] = seed;
      return seed;
    } else if (_fns.find(name) == _fns.end()) {
      // New function:
      auto replacement = get_next_name(_last_fn);
      _last_fn = replacement;
      _fns[name] = replacement;
      return replacement;
    } else {
      // Previously declared function:
      return (*_fns.find(name)).second;
    }
  }

  // Accepts a variable name, and returns the rewritten name.
  //
  std::string get_var_rewrite(const std::string& name) {
    if (_vars.empty()) {
      // First variable, seed the rewrite process:
      const auto seed = "a";
      _last_var = seed;
      _vars[name] = seed;
      return seed;
    } else if (_vars.find(name) == _vars.end()) {
      // New variable:
      auto replacement = get_next_name(_last_var);
      _last_var = replacement;
      _vars[name] = replacement;
      return replacement;
    } else {
      // Previously declared variable:
      return (*_vars.find(name)).second;
    }
  }

  // Return true if a location is in the main source file.
  bool isMainFile(const clang::SourceLocation& location) {
    auto& srcMgr = _context->getSourceManager();
    const auto file_id = srcMgr.getFileID(location).getHashValue();

    return file_id == 1;
  }

 public:
  explicit RewriterVisitor(clang::CompilerInstance *ci)
      : _context(&(ci->getASTContext())) {
    rewriter.setSourceMgr(_context->getSourceManager(),
                          _context->getLangOpts());
  }

  virtual ~RewriterVisitor() {}

  // Rewrite function declarations:
  bool VisitFunctionDecl(clang::FunctionDecl *func) {
    if (!isMainFile(func->getLocation()))
      return true;

    const auto name = func->getNameInfo().getName().getAsString();
    const auto replacement = get_fn_rewrite(name);

    rewriter.ReplaceText(
        func->getLocation(),
        static_cast<unsigned int>(name.length()),
        replacement);
    ++_fn_decl_rewrites_counter;

    return true;
  }

  // Rewrite variable declarations:
  bool VisitVarDecl(clang::VarDecl *decl) {
    if (!isMainFile(decl->getLocation()))
      return true;

    if (auto d = clang::dyn_cast<clang::NamedDecl>(decl)) {
      const auto name = d->getNameAsString();
      const auto replacement = get_var_rewrite(name);

      rewriter.ReplaceText(
          decl->getLocation(),
          static_cast<unsigned int>(name.length()),
          replacement);
      ++_var_decl_rewrites_counter;
    }

    return true;
  }

  bool VisitStmt(clang::Stmt *st) {
    if (!isMainFile(st->getLocStart()))
      return true;

    // Rewrite function calls:
    if (auto call = clang::dyn_cast<clang::CallExpr>(st)) {
      const auto callee = call->getDirectCallee();
      if (callee) {
        const auto name = callee->getNameInfo().getName().getAsString();
        const auto it = _fns.find(name);
        if (it != _fns.end()) {
          const auto replacement = (*it).second;

          rewriter.ReplaceText(
              call->getLocStart(),
              static_cast<unsigned int>(name.length()),
              replacement);
          ++_fn_call_rewrites_counter;
        }  // else function name is externally defined
      }  // else not a direct callee (what does that mean?)
    }

    // Rewrite variable names:
    if (auto *ref = clang::dyn_cast<clang::DeclRefExpr>(st)) {
      const auto name = ref->getNameInfo().getName().getAsString();
      const auto it = _vars.find(name);
      if (it != _vars.end()) {
        const auto replacement = (*it).second;
        rewriter.ReplaceText(
            ref->getLocStart(),
            static_cast<unsigned int>(name.length()),
            replacement);
        ++_var_use_rewrites_counter;
      }  // else variable name is externally defined
    }

    return true;
  }
};


class RewriterASTConsumer : public clang::ASTConsumer {
 private:
  RewriterVisitor *visitor;

 public:
  // override the constructor in order to pass CI
  explicit RewriterASTConsumer(clang::CompilerInstance *ci)
      : visitor(new RewriterVisitor(ci))
  { }

  // override this to call our RewriterVisitor on the entire source file
  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
    /* we can use ASTContext to get the TranslationUnitDecl, which is
       a single Decl that collectively represents the entire source file */
    visitor->TraverseDecl(Context.getTranslationUnitDecl());
  }
};


class RewriterFrontendAction : public clang::ASTFrontendAction {
 public:
  virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
      clang::CompilerInstance& ci, StringRef file) {
    return llvm::make_unique<RewriterASTConsumer>(&ci);
  }
};


}  // namespace rewriter


int main(int argc, const char** argv) {
  clang::tooling::CommonOptionsParser op(argc, argv, rewriter::_tool_category);
  clang::tooling::ClangTool tool(op.getCompilations(), op.getSourcePathList());

  const auto result = tool.run(
      clang::tooling::newFrontendActionFactory<
        rewriter::RewriterFrontendAction>().get());

  const auto& id = rewriter::rewriter.getSourceMgr().getMainFileID();

  if (!rewriter::isRewritten()) {
    llvm::errs() << "fatal: nothing to rewrite!";
    return EUGLY_CODE;
  }

  llvm::errs() << "\nRewrote " << rewriter::_fn_decl_rewrites_counter
               << " function declarations\n"
               << "Rewrote " << rewriter::_fn_call_rewrites_counter
               << " function calls\n\n"
               << "Rewrote " << rewriter::_var_decl_rewrites_counter
               << " variable declarations\n"
               << "Rewrote " << rewriter::_var_use_rewrites_counter
               << " variable uses\n";

  rewriter::rewriter.getEditBuffer(id).write(llvm::outs());
  return result;
}