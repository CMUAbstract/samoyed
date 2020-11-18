#include "include/SamoyedPass.h"

str_set accelReadSet;
str_set accelWriteSet;

bool compareCalledFuncName(Instruction* I, std::string funcName) {
	if (CallInst* ci = dyn_cast<CallInst>(I)) {
		for (unsigned i = 0; i < ci->getNumOperands(); ++i) {
			if (BitCastOperator* bc = dyn_cast<BitCastOperator>(ci->getOperand(i))) {
				if (!bc->getOperand(0)->getName().str().compare(funcName)) {
					return true;
				}
			}
		}
	}
	return false;
}

bool atomicStart(Instruction* I) {
	return compareCalledFuncName(I, "IF_ATOMIC");
}

bool atomicEnd(Instruction* I) {
	return compareCalledFuncName(I, "ELSE")
		|| compareCalledFuncName(I, "END_IF");
}

void getPossibleReadAddr(Instruction* I, val_set &readSet) {
	Value* v = NULL;
	// 1. All the load
	if (LoadInst* ld = dyn_cast<LoadInst>(I)) {
		v = I->getOperand(0);
	}
	// 2. Unknown function pointer arg

	// 3. Accelerator
	else if (StoreInst* st = dyn_cast<StoreInst>(I)) {
		if (st->isVolatile()) {
			Value* val = st->getValueOperand();
			Value* ptr = st->getPointerOperand();
			if (accelReadSet.find(ptr->getName().str()) != accelReadSet.end()) {
				v = val;
			}
		}
	}

	// Do not care about function local vars, i.e., allocas
	// because the lifetime is within the function
	if (v != NULL && !dyn_cast<AllocaInst>(v)) {
		readSet.insert(v);
	}
}

void getPossibleWriteAddr(Instruction* I, val_set &writeSet) {
	Value* v = NULL;
	// 1. All the str
	if (StoreInst* st = dyn_cast<StoreInst>(I)) {
		v = I->getOperand(1);
	}
	// TODO: tmp
	if (v != NULL && !dyn_cast<AllocaInst>(v)) {
		writeSet.insert(v);
	}
	// 2. Unknown function pointer arg
	// 3. Accelerator
	//if (StoreInst* st = dyn_cast<StoreInst>(I)) {
	//	if (st->isVolatile()) {
	//		Value* val = st->getValueOperand();
	//		if (PtrToIntInst* pi = dyn_cast<PtrToIntInst>(val)) {
	//			errs() << "Possible WAR to " << *I << "\n";
	//		}
	//	}
	//}
	if (StoreInst* st = dyn_cast<StoreInst>(I)) {
		if (st->isVolatile()) {
			Value* val = st->getValueOperand();
			Value* ptr = st->getPointerOperand();
			if (accelWriteSet.find(ptr->getName().str()) != accelWriteSet.end()) {
				v = val;
			}
		}
	}

	// Do not care about function local vars, i.e., allocas
	// because the lifetime is within the function
//	if (!dyn_cast<AllocaInst>(v)) {
	if (v != NULL && !dyn_cast<AllocaInst>(v)) {
		writeSet.insert(v);
	}
}

void findWAR(Module& M) {
	bool atomicRegion {false};
	val_set readSet;
	val_set writeSet;
	for (auto &F : M) {
		for (auto &B: F) {
			for (auto &I : B) {
				if (atomicStart(&I)) {
					assert(!atomicRegion && "Already in Atomic Region!");
					atomicRegion = true;
					errs() << I << "\n";
					errs() << "Atomic region begin!\n";
					readSet.clear();
					writeSet.clear();
				}
				else if (atomicEnd(&I)) {
					//assert(atomicRegion && "Not in Atomic Region!");
					atomicRegion = false;
					errs() << I << "\n";
					errs() << "Atomic region end!\n";
					for (val_set::iterator VI = readSet.begin(), VE = readSet.end(); VI != VE; ++VI) {
						errs() << "R: " << *(*VI) << "\n";
					}
					for (val_set::iterator VI = writeSet.begin(), VE = writeSet.end(); VI != VE; ++VI) {
						errs() << "W: " << *(*VI) << "\n";
					}
				}
				else if (atomicRegion) {
					// find every store to volatile
					// what about asm?
					getPossibleReadAddr(&I, readSet);
					getPossibleWriteAddr(&I, writeSet);
				}
			}
		}
	}
}


/*
 * Top pass for Samoyed
 */
bool SamoyedModulePass::runOnModule(Module &M) {
	setModule(&M);

	// Declare functions and globals in library for access
	errs() << "Samoyed Module Pass\n";

	// TODO: TEMP
	// Known read and write of accels as input
	accelReadSet.insert("DMA1SA");
	accelWriteSet.insert("DMA1DA");

	findWAR(*getModule());

	return false;
}

char SamoyedModulePass::ID = 0;

RegisterPass<SamoyedModulePass> X("samoyed", "Samoyed Pass");
