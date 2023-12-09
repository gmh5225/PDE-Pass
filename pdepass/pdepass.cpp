//===-- Partial Dead Code Elimination Pass -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
//
// EECS583 F23 - Project Group 25
//               Alex Morton, Matthew Chandra, Sam Zayko, Carter Galbus, John Jepko
//               The passes get registered as "fplicm-correctness" and "pde".
//
////===-------------------------------------------------------------------===//
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopIterator.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/LoopUtils.h"

/* *******Implementation Starts Here******* */
// You can include more Header files here
#include <vector>
#include <map>
/* *******Implementation Ends Here******* */

using namespace llvm;

namespace {
  struct HW2CorrectnessPass : public PassInfoMixin<HW2CorrectnessPass> {

    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
      llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
      /* *******Implementation Starts Here******* */
      // Your core logic should reside here.

      std::vector<BasicBlock*> freqPath;
      std::map<BasicBlock*, bool> freqPathMap;
      std::vector<Instruction*> instructionsToHoist;

      for (Loop *L : li) {

        BasicBlock *header = L->getHeader();
        BasicBlock *latch = L->getLoopLatch();
        BasicBlock *preHeader = L->getLoopPreheader();

        //llvm::errs() << "header:\n" << *header << "\n\n";
        //llvm::errs() << "latch:\n" << *latch << "\n\n";

        //Build frequent path data structures
        BasicBlock *current = header;
        freqPath.push_back(current);
        freqPathMap[current] = true;

        //llvm::errs() << "current before loop:\n" << *current << "\n\n";
        while(current != latch) {       

          for (BasicBlock * Succ : successors(current)) {

            auto probability = bpi.getEdgeProbability(current, Succ);
            uint32_t numerator = probability.getNumerator();
            uint32_t denominator = probability.getDenominator();
            float ratio = 1.0 * numerator / denominator;
            //llvm::errs() << "checking hotness of succ:\n" << *Succ << "\n\n";
            //llvm::errs() << "hot?: " << numerator << "/" << denominator << " = " << ratio << " so " << bpi.isEdgeHot(current, Succ) <<"\n\n";

            if (bpi.isEdgeHot(current, Succ)) {
            //if (ratio >= 0.8) {  
              current = Succ;
              llvm::errs() << "current updated:\n" << *current << "\n\n";
              freqPath.push_back(current);
              freqPathMap[current] = true;
              break;
            }
          }
          
        }

        //Identify hoistable loads

        for (int i = 0; i < freqPath.size(); i++) {
          BasicBlock* currentBlock = freqPath[i];
          for(Instruction &instruction: *currentBlock) {
            std::string opCodeName = instruction.getOpcodeName();
            if (opCodeName == "load") {
                bool canHoist = false;
                //llvm::errs() << "found load:\n" << instruction << "\n";

                auto value = instruction.getOperand(0);
                //llvm::errs() << "load's pointer operand:\n" << *value << "\n";

                //llvm::errs() << "All of the users:\n";
                for(auto user : value->users()){              
                  if (auto user_instruction = dyn_cast<Instruction>(user)){
                    //llvm::errs() << *user_instruction << "\n";
                    std::string userOpCodeName = user_instruction->getOpcodeName();
                    if (userOpCodeName == "store") {
                      canHoist = true;
                      //llvm::errs() << *user_instruction << "\n";
                      auto parentBlock = user_instruction->getParent();
                      if (freqPathMap[parentBlock]) {
                        //llvm::errs() << "is on frequent path\n";
                        canHoist = false;
                        break;
                      }
                    }
                  }
                }

                if (canHoist) {
                  //llvm::errs() << "We're hoisting!\n";
                  instructionsToHoist.push_back(&instruction);
                  //instruction.moveAfter(L->getLoopPreheader()->getTerminator());
                }

            }
            //llvm::errs() << "\n";
          }
        }

        //Now do the hoists
        for(int i = 0; i < instructionsToHoist.size(); i++) {
          Instruction* instruction = instructionsToHoist[i];
          LoadInst* load = dyn_cast<LoadInst>(instruction);
          BasicBlock* blockOfHoistedInstruction = instruction->getParent();

          //llvm::errs() << "looppreheader before:\n" << *preHeader << "\n" << *blockOfHoistedInstruction << "\n\n";

          AllocaInst* alloca = new AllocaInst(load->getType(), 0, nullptr, load->getAlign(), "", preHeader->getTerminator());
          //pa->insertBefore(instruction);
          //llvm::errs() << "alloca after:\n" << *preHeader << "\n" << *blockOfHoistedInstruction << "\n\n";

          LoadInst* newLoad = new LoadInst(load->getType(), alloca, "", instruction);
          //llvm::errs() << "new load after:\n" << *preHeader << "\n" << *blockOfHoistedInstruction<< "\n\n";

          instruction->moveBefore(preHeader->getTerminator());
          //llvm::errs() << "looppreheader after:\n" << *preHeader << "\n" << *blockOfHoistedInstruction << "\n\n";
          //auto *pa = new AllocaInst(instruction->getType(), 0, "indexLoc");
          
          StoreInst* store = new StoreInst(instruction, alloca, preHeader->getTerminator());
          //llvm::errs() << "store after:\n" << *preHeader << "\n" << *blockOfHoistedInstruction << "\n\n";

          /*llvm::errs() << "users of load:\n";
          for(auto user : load->users()){              
            if (auto user_instruction = dyn_cast<Instruction>(user)){
              llvm::errs() << *user_instruction << "\n";
            }
          }*/
          
          instruction->replaceUsesOutsideBlock(newLoad, preHeader);
          //llvm::errs() << "update refs after:\n" << *preHeader << "\n" << *blockOfHoistedInstruction << "\n\n";

          auto value = instruction->getOperand(0);
          value->replaceUsesOutsideBlock(alloca, preHeader);
          
          /*llvm::errs() << "users of new load:\n";
          for(auto user : newLoad->users()){              
            if (auto user_instruction = dyn_cast<Instruction>(user)){
              llvm::errs() << *user_instruction << "\n";
            }
          }*/

        }

        //cleanup

      }


      /* *******Implementation Ends Here******* */
      // Your pass is modifying the source code. Figure out which analyses
      // are preserved and only return those, not all.
      return PreservedAnalyses::all();
    }
  };
  struct PDEPass : public PassInfoMixin<PDEPass> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
      llvm::BlockFrequencyAnalysis::Result &bfi = FAM.getResult<BlockFrequencyAnalysis>(F);
      llvm::BranchProbabilityAnalysis::Result &bpi = FAM.getResult<BranchProbabilityAnalysis>(F);
      llvm::LoopAnalysis::Result &li = FAM.getResult<LoopAnalysis>(F);
      /* *******Implementation Starts Here******* */
      // This is a bonus. You do not need to attempt this to receive full credit.
      /* *******Implementation Ends Here******* */

      // Your pass is modifying the source code. Figure out which analyses
      // are preserved and only return those, not all.
      return PreservedAnalyses::all();
    }
  };
}

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "HW2Pass", "v0.1",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
        ArrayRef<PassBuilder::PipelineElement>) {
          if(Name == "fplicm-correctness"){
            FPM.addPass(HW2CorrectnessPass());
            return true;
          }
          if(Name == "pde"){
            FPM.addPass(PDEPass());
            return true;
          }
          return false;
        }
      );
    }
  };
}