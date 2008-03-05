//==- GRCoreEngine.cpp - Path-Sensitive Dataflow Engine ----------------*- C++ -*-//
//             
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines a generic engine for intraprocedural, path-sensitive,
//  dataflow analysis via graph reachability engine.
//
//===----------------------------------------------------------------------===//

#include "clang/Analysis/PathSensitive/GRCoreEngine.h"
#include "clang/AST/Expr.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/DenseMap.h"
#include <vector>

using llvm::cast;
using llvm::isa;
using namespace clang;

namespace {
  class VISIBILITY_HIDDEN DFS : public GRWorkList {
  llvm::SmallVector<GRWorkListUnit,20> Stack;
public:
  virtual bool hasWork() const {
    return !Stack.empty();
  }

  virtual void Enqueue(const GRWorkListUnit& U) {
    Stack.push_back(U);
  }

  virtual GRWorkListUnit Dequeue() {
    assert (!Stack.empty());
    const GRWorkListUnit& U = Stack.back();
    Stack.pop_back(); // This technically "invalidates" U, but we are fine.
    return U;
  }
};
} // end anonymous namespace

// Place the dstor for GRWorkList here because it contains virtual member
// functions, and we the code for the dstor generated in one compilation unit.
GRWorkList::~GRWorkList() {}

GRWorkList* GRWorkList::MakeDFS() { return new DFS(); }

/// ExecuteWorkList - Run the worklist algorithm for a maximum number of steps.
bool GRCoreEngineImpl::ExecuteWorkList(unsigned Steps) {
  
  if (G->num_roots() == 0) { // Initialize the analysis by constructing
    // the root if none exists.
    
    CFGBlock* Entry = &getCFG().getEntry();
    
    assert (Entry->empty() && 
            "Entry block must be empty.");
    
    assert (Entry->succ_size() == 1 &&
            "Entry block must have 1 successor.");
    
    // Get the solitary successor.
    CFGBlock* Succ = *(Entry->succ_begin());   
    
    // Construct an edge representing the
    // starting location in the function.
    BlockEdge StartLoc(getCFG(), Entry, Succ);
    
    // Set the current block counter to being empty.
    WList->setBlockCounter(BCounterFactory.GetEmptyCounter());
    
    // Generate the root.
    GenerateNode(StartLoc, getInitialState());
  }
  
  while (Steps && WList->hasWork()) {
    --Steps;
    const GRWorkListUnit& WU = WList->Dequeue();
    
    // Set the current block counter.
    WList->setBlockCounter(WU.getBlockCounter());

    // Retrieve the node.
    ExplodedNodeImpl* Node = WU.getNode();
    
    // Dispatch on the location type.
    switch (Node->getLocation().getKind()) {
      default:
        assert (isa<BlockEdge>(Node->getLocation()));
        HandleBlockEdge(cast<BlockEdge>(Node->getLocation()), Node);
        break;
        
      case ProgramPoint::BlockEntranceKind:
        HandleBlockEntrance(cast<BlockEntrance>(Node->getLocation()), Node);
        break;
        
      case ProgramPoint::BlockExitKind:
        assert (false && "BlockExit location never occur in forward analysis.");
        break;
        
      case ProgramPoint::PostStmtKind:
        HandlePostStmt(cast<PostStmt>(Node->getLocation()), WU.getBlock(),
                       WU.getIndex(), Node);
        break;        
    }
  }
  
  return WList->hasWork();
}

void GRCoreEngineImpl::HandleBlockEdge(const BlockEdge& L,
                                       ExplodedNodeImpl* Pred) {
  
  CFGBlock* Blk = L.getDst();
  
  // Check if we are entering the EXIT block. 
  if (Blk == &getCFG().getExit()) {
    
    assert (getCFG().getExit().size() == 0 
            && "EXIT block cannot contain Stmts.");

    // Process the final state transition.    
    void* State = ProcessEOP(Blk, Pred->State);

    bool IsNew;
    ExplodedNodeImpl* Node = G->getNodeImpl(BlockEntrance(Blk), State, &IsNew);
    Node->addPredecessor(Pred);
    
    // If the node was freshly created, mark it as an "End-Of-Path" node.
    if (IsNew) G->addEndOfPath(Node); 
    
    // This path is done. Don't enqueue any more nodes.
    return;
  }

  // FIXME: Should we allow ProcessBlockEntrance to also manipulate state?
  
  if (ProcessBlockEntrance(Blk, Pred->State, WList->getBlockCounter()))
    GenerateNode(BlockEntrance(Blk), Pred->State, Pred);
}

void GRCoreEngineImpl::HandleBlockEntrance(const BlockEntrance& L,
                                           ExplodedNodeImpl* Pred) {
  
  // Increment the block counter.
  GRBlockCounter Counter = WList->getBlockCounter();
  Counter = BCounterFactory.IncrementCount(Counter, L.getBlock()->getBlockID());
  WList->setBlockCounter(Counter);
  
  // Process the entrance of the block.  
  if (Stmt* S = L.getFirstStmt()) {
    GRStmtNodeBuilderImpl Builder(L.getBlock(), 0, Pred, this);
    ProcessStmt(S, Builder);
  }
  else 
    HandleBlockExit(L.getBlock(), Pred);
}


void GRCoreEngineImpl::HandleBlockExit(CFGBlock * B, ExplodedNodeImpl* Pred) {
  
  if (Stmt* Term = B->getTerminator()) {
    switch (Term->getStmtClass()) {
      default:
        assert(false && "Analysis for this terminator not implemented.");
        break;
                
      case Stmt::BinaryOperatorClass: // '&&' and '||'
        HandleBranch(cast<BinaryOperator>(Term)->getLHS(), Term, B, Pred);
        return;
        
      case Stmt::ConditionalOperatorClass:
        HandleBranch(cast<ConditionalOperator>(Term)->getCond(), Term, B, Pred);
        return;
        
        // FIXME: Use constant-folding in CFG construction to simplify this
        // case.
        
      case Stmt::ChooseExprClass:
        HandleBranch(cast<ChooseExpr>(Term)->getCond(), Term, B, Pred);
        return;
        
      case Stmt::DoStmtClass:
        HandleBranch(cast<DoStmt>(Term)->getCond(), Term, B, Pred);
        return;
        
      case Stmt::ForStmtClass:
        HandleBranch(cast<ForStmt>(Term)->getCond(), Term, B, Pred);
        return;
      
      case Stmt::ContinueStmtClass:
      case Stmt::BreakStmtClass:
      case Stmt::GotoStmtClass:        
        break;
        
      case Stmt::IfStmtClass:
        HandleBranch(cast<IfStmt>(Term)->getCond(), Term, B, Pred);
        return;
               
      case Stmt::IndirectGotoStmtClass: {
        // Only 1 successor: the indirect goto dispatch block.
        assert (B->succ_size() == 1);
        
        GRIndirectGotoNodeBuilderImpl
           builder(Pred, B, cast<IndirectGotoStmt>(Term)->getTarget(),
                   *(B->succ_begin()), this);
        
        ProcessIndirectGoto(builder);
        return;
      }
        
      case Stmt::SwitchStmtClass: {
        GRSwitchNodeBuilderImpl builder(Pred, B,
                                        cast<SwitchStmt>(Term)->getCond(),
                                        this);
        
        ProcessSwitch(builder);
        return;
      }
        
      case Stmt::WhileStmtClass:
        HandleBranch(cast<WhileStmt>(Term)->getCond(), Term, B, Pred);
        return;
    }
  }

  assert (B->succ_size() == 1 &&
          "Blocks with no terminator should have at most 1 successor.");
    
  GenerateNode(BlockEdge(getCFG(),B,*(B->succ_begin())), Pred->State, Pred);
}

void GRCoreEngineImpl::HandleBranch(Expr* Cond, Stmt* Term, CFGBlock * B,
                                ExplodedNodeImpl* Pred) {
  assert (B->succ_size() == 2);

  GRBranchNodeBuilderImpl Builder(B, *(B->succ_begin()), *(B->succ_begin()+1),
                                  Pred, this);
  
  ProcessBranch(Cond, Term, Builder);
}

void GRCoreEngineImpl::HandlePostStmt(const PostStmt& L, CFGBlock* B,
                                  unsigned StmtIdx, ExplodedNodeImpl* Pred) {
  
  assert (!B->empty());

  if (StmtIdx == B->size())
    HandleBlockExit(B, Pred);
  else {
    GRStmtNodeBuilderImpl Builder(B, StmtIdx, Pred, this);
    ProcessStmt((*B)[StmtIdx], Builder);
  }
}

typedef llvm::DenseMap<Stmt*,Stmt*> ParentMapTy;
/// PopulateParentMap - Recurse the AST starting at 'Parent' and add the
///  mappings between child and parent to ParentMap.
static void PopulateParentMap(Stmt* Parent, ParentMapTy& M) {
  for (Stmt::child_iterator I=Parent->child_begin(), 
       E=Parent->child_end(); I!=E; ++I) {
    
    assert (M.find(*I) == M.end());
    M[*I] = Parent;
    PopulateParentMap(*I, M);
  }
}

/// GenerateNode - Utility method to generate nodes, hook up successors,
///  and add nodes to the worklist.
void GRCoreEngineImpl::GenerateNode(const ProgramPoint& Loc, void* State,
                                ExplodedNodeImpl* Pred) {
  
  bool IsNew;
  ExplodedNodeImpl* Node = G->getNodeImpl(Loc, State, &IsNew);
  
  if (Pred) 
    Node->addPredecessor(Pred);  // Link 'Node' with its predecessor.
  else {
    assert (IsNew);
    G->addRoot(Node);  // 'Node' has no predecessor.  Make it a root.
  }
  
  // Only add 'Node' to the worklist if it was freshly generated.
  if (IsNew) WList->Enqueue(Node);
}

GRStmtNodeBuilderImpl::GRStmtNodeBuilderImpl(CFGBlock* b, unsigned idx,
                                     ExplodedNodeImpl* N, GRCoreEngineImpl* e)
  : Eng(*e), B(*b), Idx(idx), LastNode(N), Populated(false) {
  Deferred.insert(N);
}

GRStmtNodeBuilderImpl::~GRStmtNodeBuilderImpl() {
  for (DeferredTy::iterator I=Deferred.begin(), E=Deferred.end(); I!=E; ++I)
    if (!(*I)->isSink())
      GenerateAutoTransition(*I);
}

void GRStmtNodeBuilderImpl::GenerateAutoTransition(ExplodedNodeImpl* N) {
  assert (!N->isSink());
  
  PostStmt Loc(getStmt());
  
  if (Loc == N->getLocation()) {
    // Note: 'N' should be a fresh node because otherwise it shouldn't be
    // a member of Deferred.
    Eng.WList->Enqueue(N, B, Idx+1);
    return;
  }
  
  bool IsNew;
  ExplodedNodeImpl* Succ = Eng.G->getNodeImpl(Loc, N->State, &IsNew);
  Succ->addPredecessor(N);

  if (IsNew)
    Eng.WList->Enqueue(Succ, B, Idx+1);
}

ExplodedNodeImpl* GRStmtNodeBuilderImpl::generateNodeImpl(Stmt* S, void* State,
                                                      ExplodedNodeImpl* Pred) {
  
  bool IsNew;
  ExplodedNodeImpl* N = Eng.G->getNodeImpl(PostStmt(S), State, &IsNew);
  N->addPredecessor(Pred);
  Deferred.erase(Pred);
  
  HasGeneratedNode = true;
  
  if (IsNew) {
    Deferred.insert(N);
    LastNode = N;
    return N;
  }
  
  LastNode = NULL;
  return NULL;  
}

ExplodedNodeImpl* GRBranchNodeBuilderImpl::generateNodeImpl(void* State,
                                                            bool branch) {  
  bool IsNew;
  
  ExplodedNodeImpl* Succ =
    Eng.G->getNodeImpl(BlockEdge(Eng.getCFG(), Src, branch ? DstT : DstF),
                       State, &IsNew);
  
  Succ->addPredecessor(Pred);
  
  if (branch) GeneratedTrue = true;
  else GeneratedFalse = true;  
  
  if (IsNew) {
    Deferred.push_back(Succ);
    return Succ;
  }
  
  return NULL;
}

GRBranchNodeBuilderImpl::~GRBranchNodeBuilderImpl() {
  if (!GeneratedTrue) generateNodeImpl(Pred->State, true);
  if (!GeneratedFalse) generateNodeImpl(Pred->State, false);
  
  for (DeferredTy::iterator I=Deferred.begin(), E=Deferred.end(); I!=E; ++I)
    if (!(*I)->isSink()) Eng.WList->Enqueue(*I);
}


ExplodedNodeImpl*
GRIndirectGotoNodeBuilderImpl::generateNodeImpl(const Iterator& I,
                                                void* St,
                                                bool isSink) {
  bool IsNew;
  
  ExplodedNodeImpl* Succ =
    Eng.G->getNodeImpl(BlockEdge(Eng.getCFG(), Src, I.getBlock(), true),
                       St, &IsNew);
              
  Succ->addPredecessor(Pred);
  
  if (IsNew) {
    
    if (isSink)
      Succ->markAsSink();
    else
      Eng.WList->Enqueue(Succ);
    
    return Succ;
  }
                       
  return NULL;
}


ExplodedNodeImpl*
GRSwitchNodeBuilderImpl::generateCaseStmtNodeImpl(const Iterator& I, void* St) {

  bool IsNew;
  
  ExplodedNodeImpl* Succ = Eng.G->getNodeImpl(BlockEdge(Eng.getCFG(), Src,
                                                        I.getBlock()),
                                              St, &IsNew);  
  Succ->addPredecessor(Pred);
  
  if (IsNew) {
    Eng.WList->Enqueue(Succ);
    return Succ;
  }
  
  return NULL;
}


ExplodedNodeImpl*
GRSwitchNodeBuilderImpl::generateDefaultCaseNodeImpl(void* St, bool isSink) {
  
  // Get the block for the default case.
  assert (Src->succ_rbegin() != Src->succ_rend());
  CFGBlock* DefaultBlock = *Src->succ_rbegin();
  
  bool IsNew;
  
  ExplodedNodeImpl* Succ = Eng.G->getNodeImpl(BlockEdge(Eng.getCFG(), Src,
                                                        DefaultBlock),
                                              St, &IsNew);  
  Succ->addPredecessor(Pred);
  
  if (IsNew) {
    if (isSink)
      Succ->markAsSink();
    else
      Eng.WList->Enqueue(Succ);
    
    return Succ;
  }
  
  return NULL;
}
