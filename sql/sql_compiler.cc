#include "sql_compiler.h"
#include "KaleidoscopeJIT.h"

#include "sql_optimizer.h" // JOIN

#include <llvm/ADT/ArrayRef.h>

#include <llvm/ExecutionEngine/GenericValue.h>

#include <llvm/IR/InstIterator.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>

#include <llvm/IRReader/IRReader.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

using namespace llvm;
using namespace llvm::orc;

Function *patchSubSelect(std::unique_ptr<Module> &Owner, QEP_TAB *qep_tab);

static Function *cloneFunc(std::unique_ptr<Module> &Owner, std::string Name)
{
  Function *OriginalFunc = Owner->getFunction(Name);
  ValueToValueMapTy VMap;
  Function *SpecializedFunc = CloneFunction(OriginalFunc, VMap);
  SpecializedFunc->setName(Name + "JIT");
  return SpecializedFunc;
}

void createNewCall(CallInst *OldCallInst, Function *NewFunc)
{
  CallSite CS(OldCallInst);
  SmallVector<llvm::Value *, 8> Args(CS.arg_begin(), CS.arg_end());
  CallInst *NewCI = CallInst::Create(NewFunc, Args);
  NewCI->setCallingConv(NewFunc->getCallingConv());
  if (!OldCallInst->use_empty())
    OldCallInst->replaceAllUsesWith(NewCI);
  ReplaceInstWithInst(OldCallInst, NewCI);
}

Function *patchEvaluateJoinRecord(std::unique_ptr<Module> &Owner, QEP_TAB *qep_tab)
{
  Function *SpecializedFunc = cloneFunc(Owner, "evaluate_join_record");

  // Find calls to join->next_select
  CallInst *NextSelectCallInst;
  PointerType *JOIN_Ty = Owner->getTypeByName("class.JOIN")->getPointerTo();
  PointerType *QEP_TAB_Ty = Owner->getTypeByName("class.QEP_TAB")->getPointerTo();
  IntegerType *Int1_Ty = IntegerType::get(Owner->getContext(), 1);
  for (inst_iterator I = inst_begin(SpecializedFunc),  E = inst_end(SpecializedFunc); I != E; I++) {
    if (CallInst *C = dyn_cast<CallInst>(&*I)) {
      /* it is an indirect call, so getCalledFunction() should be null */
      if (C->getCalledFunction() == nullptr) {
        FunctionType *FT = C->getFunctionType();
        if (FT->getReturnType() != IntegerType::get(Owner->getContext(), 32))
          continue;
        if (FT->getNumParams() != 3)
          continue;
        if (FT->getParamType(0) != JOIN_Ty || FT->getParamType(1) != QEP_TAB_Ty || FT->getParamType(2) != Int1_Ty)
          continue;
        NextSelectCallInst = C;
        break;
      }
    }
  }

  if (!NextSelectCallInst) {
      errs() << "[ERROR] Could not find next_select call in evaluate_join_record. Aborting compilation\n";
      return nullptr;
  }

  Function *SpecializedNextSelect = nullptr;
  if (qep_tab->next_select == sub_select)
      SpecializedNextSelect = patchSubSelect(Owner, qep_tab+1);

  if (SpecializedNextSelect)
      createNewCall(NextSelectCallInst, SpecializedNextSelect);

  if(verifyFunction(*SpecializedFunc, &errs())) {
    errs() << "Error specializing function evaluate_join_record\n";
    errs() << *SpecializedFunc;
    return nullptr;
  }

  return SpecializedFunc;
}

Function *patchSubSelectOp(std::unique_ptr<Module> &Owner, QEP_TAB *qep_tab)
{
  Function *SpecializedFunc = cloneFunc(Owner, "sub_select_op");
  return SpecializedFunc;
}

Function *patchSubSelect(std::unique_ptr<Module> &Owner, QEP_TAB *qep_tab)
{
  Function *SpecializedFunc = cloneFunc(Owner, "sub_select");

  // Find calls to read_first_record and read_record
  CallInst *ReadFirstRecordCallInst = nullptr, *ReadRecordCallInst = nullptr;
  PointerType *JOIN_Ty = Owner->getTypeByName("class.JOIN")->getPointerTo();
  PointerType *QEP_TAB_Ty = Owner->getTypeByName("class.QEP_TAB")->getPointerTo();
  PointerType *READ_RECORD_Ty = Owner->getTypeByName("struct.READ_RECORD")->getPointerTo();
  for (inst_iterator I = inst_begin(SpecializedFunc),  E = inst_end(SpecializedFunc); I != E; I++) {
    if (CallInst *C = dyn_cast<CallInst>(&*I)) {
      /* they are indirect calls, so getCalledFunction() should be null */
      if (C->getCalledFunction() == nullptr) {
        FunctionType *FT = C->getFunctionType();
        if (FT->getReturnType() != IntegerType::get(Owner->getContext(), 32))
          continue;
        if (FT->getNumParams() != 1)
          continue;
        if (FT->getParamType(0) == QEP_TAB_Ty)
          // I'm getting 3 here: 100, 278, 344
          errs() << *C << "\n";
        else if (FT->getParamType(0) == READ_RECORD_Ty)
          errs() << *C << "\n";
        //// end_select func also has the same signature, but the QEP_TAB parameter is called with null. Filter these out
        //if (C->getArgOperand(1) == Constant::getNullValue(QEP_TAB_Ty))
        //  continue;
        //CallInsts.push_back(C);
        //found++;
      }
    }
  }

  errs() << ReadFirstRecordCallInst << ReadRecordCallInst << JOIN_Ty << "\n";

  // patch read_first_record
  Function *ReadFirstRecordFunction = nullptr;
  // TODO: add support for other functions
//  if (qep_tab->read_first_record == read_first_record_seq)
//      ReadFirstRecordFunction = Owner->getFunction("read_first_record_seq");
//  
//  if (ReadFirstRecordFunction)
//      createNewCall(ReadFirstRecordCallInst, ReadFirstRecordFunction);
//  else
//      errs() << "[WARNING] Could not patch read_first_record: function not supported.\n";
//
//  // patch read_record
//  Function *ReadRecordFunction = nullptr;
//  // TODO: add support for other functions
//  if (qep_tab->read_record.read_record == join_read_next_same)
//      ReadRecordFunction = Owner->getFunction("join_read_next_same");
//
//  if (ReadRecordFunction)
//      createNewCall(ReadRecordCallInst, ReadRecordFunction);
//  else
//      errs() << "[WARNING] Could not patch read_record: function not supported.\n";


  // Find call to evaluate_join_record
  std::vector<CallInst*> EvaluateJoinRecordCallInsts;
  for (inst_iterator I = inst_begin(SpecializedFunc),  E = inst_end(SpecializedFunc); I != E; I++) {
    if (CallInst *C = dyn_cast<CallInst>(&*I)) {
      Function *F = C->getCalledFunction();
      if (F && F->getName() == "evaluate_join_record") {
          EvaluateJoinRecordCallInsts.push_back(C);
      }
    }
  }
  errs() << "\n\n\n[DEBUG] sub_select had " << EvaluateJoinRecordCallInsts.size() << " calls to evaluate_join_record\n";

  Function *SpecializedEvaluateJoinRecord = patchEvaluateJoinRecord(Owner, qep_tab);
  for (int i = 0; i < EvaluateJoinRecordCallInsts.size(); i++)
      createNewCall(EvaluateJoinRecordCallInsts[i], SpecializedEvaluateJoinRecord);

  if(verifyFunction(*SpecializedFunc, &errs())) {
    errs() << "Error specializing function sub_select\n";
    errs() << *SpecializedFunc;
    return nullptr;
  }

  return SpecializedFunc;
}

Function *patchDoSelect(std::unique_ptr<Module> &Owner, JOIN *plan)
{
  Function *SpecializedFunc = cloneFunc(Owner, "do_select");

  // Find calls to join->first_select
  std::vector<CallInst*> CallInsts;
  PointerType *JOIN_Ty = Owner->getTypeByName("class.JOIN")->getPointerTo();
  PointerType *QEP_TAB_Ty = Owner->getTypeByName("class.QEP_TAB")->getPointerTo();
  int found = 0;
  for (inst_iterator I = inst_begin(SpecializedFunc),  E = inst_end(SpecializedFunc); I != E && found < 2; I++) {
    if (CallInst *C = dyn_cast<CallInst>(&*I)) {
      /* it is an indirect call, so getCalledFunction() should be null */
      if (C->getCalledFunction() == nullptr) {
        FunctionType *FT = C->getFunctionType();
        if (FT->getReturnType() != IntegerType::get(Owner->getContext(), 32))
          continue;
        if (FT->getNumParams() != 3)
          continue;
        if (FT->getParamType(0) != JOIN_Ty || FT->getParamType(1) != QEP_TAB_Ty || FT->getParamType(2) != IntegerType::get(Owner->getContext(), 1))
          continue;
        // end_select func also has the same signature, but the QEP_TAB parameter is called with null. Filter these out
        if (C->getArgOperand(1) == Constant::getNullValue(QEP_TAB_Ty))
          continue;
        CallInsts.push_back(C);
        found++;
      }
    }
  }

  Function *Patched_FirstSelect = nullptr;
  if (plan->first_select == sub_select)
    Patched_FirstSelect = patchSubSelect(Owner, plan->qep_tab);

  if (Patched_FirstSelect) {
    createNewCall(CallInsts[0], Patched_FirstSelect);
    createNewCall(CallInsts[1], Patched_FirstSelect);
  }

  if(verifyFunction(*SpecializedFunc, &errs())) {
    errs() << "Error specializing function do_select\n";
    errs() << *SpecializedFunc;
    return nullptr;
  }

  return SpecializedFunc;
}

int do_select_compile(JOIN *join)
{
  //mtx.lock();
  //errs() << "\n\n--------------\n\n";
  //traverse(planstate);
  //errs() << "\n--------------\n\n";
  //mtx.unlock();
  LLVMContext TheContext;
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  SMDiagnostic error;
  errs() << "Loading file\n";
  std::unique_ptr<Module> Owner = parseIRFile("/local/data/ichilevi/git_repos/mysql57/sql_executor.cc.ll", error, TheContext);
  if (!Owner) {
    errs() << error.getMessage();
    errs() << "Could not load pre-compiled file\n";
    return false;
  }

  printf("Trying to patch\n");
  if (!patchDoSelect(Owner, join)) {
    errs() << "[ERROR] could not patch plan\n";
    return false;
  }

  static std::unique_ptr<KaleidoscopeJIT> TheJIT = llvm::make_unique<KaleidoscopeJIT>();
  auto H = TheJIT->addModule(std::move(Owner));

  auto ExprSymbol = TheJIT->findSymbol("do_select");
  if (!ExprSymbol) {
    errs() << "[ERROR] Could not find symbol\n";
    return false;
  }

  int (*doSelect) (JOIN*) = (int (*) (JOIN*)) cantFail(ExprSymbol.getAddress());
  doSelect(join);

  //void (*execPlan) (EState*, PlanState*, bool, CmdType, bool, uint64, ScanDirection, DestReceiver*, bool) =
  //  (void (*) (EState*, PlanState*, bool, CmdType, bool, uint64, ScanDirection, DestReceiver*, bool)) cantFail(ExprSymbol.getAddress());

  //execPlan(estate, planstate, use_parallel_mode, operation, sendTuples, numberTuples, direction, dest, execute_once);

  TheJIT->removeModule(H);

  errs() << "JIT executed successfully\n";
  return true;
}
