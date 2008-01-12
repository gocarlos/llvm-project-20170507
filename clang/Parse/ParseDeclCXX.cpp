//===--- ParseDeclCXX.cpp - C++ Declaration Parsing -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the C++ Declaration portions of the Parser interfaces.
//
//===----------------------------------------------------------------------===//

#include "clang/Parse/Parser.h"
#include "clang/Parse/Scope.h"
#include "clang/Basic/Diagnostic.h"
using namespace clang;

/// ParseNamespace - We know that the current token is a namespace keyword. This
/// may either be a top level namespace or a block-level namespace alias.
///
///       namespace-definition: [C++ 7.3: basic.namespace]
///         named-namespace-definition
///         unnamed-namespace-definition
///
///       unnamed-namespace-definition:
///         'namespace' attributes[opt] '{' namespace-body '}'
///
///       named-namespace-definition:
///         original-namespace-definition
///         extension-namespace-definition
///
///       original-namespace-definition:
///         'namespace' identifier attributes[opt] '{' namespace-body '}'
///
///       extension-namespace-definition:
///         'namespace' original-namespace-name '{' namespace-body '}'
///  
///       namespace-alias-definition:  [C++ 7.3.2: namespace.alias]
///         'namespace' identifier '=' qualified-namespace-specifier ';'
///
Parser::DeclTy *Parser::ParseNamespace(unsigned Context) {
  assert(Tok.is(tok::kw_namespace) && "Not a namespace!");
  SourceLocation NamespaceLoc = ConsumeToken();  // eat the 'namespace'.
  
  SourceLocation IdentLoc;
  IdentifierInfo *Ident = 0;
  
  if (Tok.is(tok::identifier)) {
    Ident = Tok.getIdentifierInfo();
    IdentLoc = ConsumeToken();  // eat the identifier.
  }
  
  // Read label attributes, if present.
  DeclTy *AttrList = 0;
  if (Tok.is(tok::kw___attribute))
    // FIXME: save these somewhere.
    AttrList = ParseAttributes();
  
  if (Tok.is(tok::equal)) {
    // FIXME: Verify no attributes were present.
    // FIXME: parse this.
  } else if (Tok.is(tok::l_brace)) {
    SourceLocation LBrace = ConsumeBrace();
    // FIXME: push a scope, push a namespace decl.
    
    while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
      // FIXME capture the decls.
      ParseExternalDeclaration();
    }
    
    SourceLocation RBrace = MatchRHSPunctuation(tok::r_brace, LBrace);
    
    // FIXME: act on this.
  } else {
    unsigned D = Ident ? diag::err_expected_lbrace : 
                         diag::err_expected_ident_lbrace;
    Diag(Tok.getLocation(), D);
  }
  
  return 0;
}

/// ParseLinkage - We know that the current token is a string_literal
/// and just before that, that extern was seen.
///
///       linkage-specification: [C++ 7.5p2: dcl.link]
///         'extern' string-literal '{' declaration-seq[opt] '}'
///         'extern' string-literal declaration
///
Parser::DeclTy *Parser::ParseLinkage(unsigned Context) {
  assert(Tok.is(tok::string_literal) && "Not a stringliteral!");
  llvm::SmallVector<char, 8> LangBuffer;
  // LangBuffer is guaranteed to be big enough.
  LangBuffer.resize(Tok.getLength());
  const char *LangBufPtr = &LangBuffer[0];
  unsigned StrSize = PP.getSpelling(Tok, LangBufPtr);

  SourceLocation Loc = ConsumeStringToken();
  DeclTy *D = 0;
  SourceLocation LBrace, RBrace;
  
  if (Tok.isNot(tok::l_brace)) {
    D = ParseDeclaration(Context);
  } else {
    LBrace = ConsumeBrace();
    while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
      // FIXME capture the decls.
      D = ParseExternalDeclaration();
    }

    RBrace = MatchRHSPunctuation(tok::r_brace, LBrace);
  }

  if (!D)
    return 0;

  return Actions.ActOnLinkageSpec(Loc, LBrace, RBrace, LangBufPtr, StrSize, D);
}
