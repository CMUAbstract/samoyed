#ifndef __SAMOYEDPASS__
#define __SAMOYEDPASS__

#include "global.h"

using namespace llvm;

class SamoyedModulePass : public ModulePass {
	public:
		static char ID;
		SamoyedModulePass() : ModulePass(ID) {}

		virtual bool runOnModule(Module &M);

		virtual void getAnalysisUsage(AnalysisUsage& AU) const {
			AU.setPreservesAll();
			AU.addRequired<AAResultsWrapperPass>();
		}
		Module* getModule() {
			return m;
		}
		void setModule(Module* _m) {
			m = _m;
		}
	private:
		Module* m;
};

#endif
