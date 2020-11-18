#ifndef __FUNCTION__
#define __FUNCTION__

#include "global.h"

typedef std::vector<std::pair<clang::Expr*, std::string>> knob_vec;
// name, type, size
typedef std::vector<std::pair<std::string, std::pair<clang::QualType, std::string>>> inout_vec;
typedef std::vector<clang::Expr*> arg_vec;
class ProtectedFunction {
	public:
		ProtectedFunction(unsigned _id, clang::FunctionDecl* _F)
			: id {_id},
			atomicCodeStart {clang::SourceLocation::getFromRawEncoding(0)},
			atomicCodeEnd {clang::SourceLocation::getFromRawEncoding(0)},
			elseCodeStart {clang::SourceLocation::getFromRawEncoding(0)},
			elseCodeEnd {clang::SourceLocation::getFromRawEncoding(0)},
			entireCodeStart {clang::SourceLocation::getFromRawEncoding(0)},
			entireCodeEnd {clang::SourceLocation::getFromRawEncoding(0)},
			func {_F}
		{
			knobs.clear();
			ins.clear();
			outs.clear();
			inouts.clear();
			fallback = "";
		}

		void setAtomicCodeStart(clang::SourceLocation _acs) {
			assert(!atomicCodeStart.getRawEncoding()
					&& "atomicCodeStart is not 0!");
			atomicCodeStart = _acs;
		}
		void setAtomicCodeEnd(clang::SourceLocation _ace) {
			assert(!atomicCodeEnd.getRawEncoding()
					&& "atomicCodeEnd is not 0!");
			atomicCodeEnd = _ace;
		}
		void setElseCodeStart(clang::SourceLocation _ecs) {
			assert(!elseCodeStart.getRawEncoding()
					&& "elseCodeStart is not 0!");
			elseCodeStart = _ecs;
		}
		void setElseCodeEnd(clang::SourceLocation _ece) {
			assert(!elseCodeEnd.getRawEncoding()
					&& "elseCodeEnd is not 0!");
			elseCodeEnd = _ece;
		}
		void addKnob(clang::Expr* knob, std::string initVal = "1") {
			knobs.push_back(std::make_pair(knob, initVal));
		}
		void addFallback(std::string _fallback) {
			fallback = _fallback;
		}
		std::string getFallback() {
			return fallback;
		}
		void addIns(std::string name, clang::QualType type, std::string size = "") {
			ins.push_back(std::make_pair(name, std::make_pair(type, size)));
		}
		void addOuts(std::string name, clang::QualType type, std::string size = "") {
			outs.push_back(std::make_pair(name, std::make_pair(type, size)));
		}
		void addInOuts(std::string name, clang::QualType type, std::string size = "") {
			inouts.push_back(std::make_pair(name, std::make_pair(type, size)));
		}
		void setEntireCodeStart(clang::SourceLocation _ecs) {
			assert(!entireCodeStart.getRawEncoding()
					&& "entireCodeStart is not 0!");
			entireCodeStart = _ecs;
		}
		void setEntireCodeEnd(clang::SourceLocation _ece) {
			assert(!entireCodeEnd.getRawEncoding()
					&& "entireCodeEnd is not 0!");
			entireCodeEnd = _ece;
		}
		size_t getNumKnobs() {
			return knobs.size();
		}
		size_t getNumIns() {
			return ins.size();
		}
		size_t getNumOuts() {
			return outs.size();
		}
		size_t getNumWARs() {
			return inouts.size();
		}
		bool isAtomicCodeStartSet() {
			if (atomicCodeStart.getRawEncoding())
				return true;
			else
				return false;
		}
		bool isAtomicCodeEndSet() {
			if (atomicCodeEnd.getRawEncoding())
				return true;
			else
				return false;
		}
		bool isElseCodeStartSet() {
			if (elseCodeStart.getRawEncoding())
				return true;
			else
				return false;
		}
		bool isElseCodeEndSet() {
			if (elseCodeEnd.getRawEncoding())
				return true;
			else
				return false;
		}
		std::string getAtomicStr(clang::Rewriter &R) {
			return R.getRewrittenText(clang::SourceRange(atomicCodeStart,
						atomicCodeEnd));
		}
		std::string getElseStr(clang::Rewriter &R) {
			return R.getRewrittenText(clang::SourceRange(elseCodeStart,
						elseCodeEnd));
		}
		std::string getEntireStr(clang::Rewriter &R) {
			return R.getRewrittenText(clang::SourceRange(entireCodeStart,
						entireCodeEnd));
		}
		clang::SourceLocation getAtomicCodeStart() {
			return atomicCodeStart;
		}
		clang::SourceLocation getAtomicCodeEnd() {
			return atomicCodeEnd;
		}
		clang::SourceLocation getElseCodeStart() {
			return elseCodeStart;
		}
		clang::SourceLocation getElseCodeEnd() {
			return elseCodeEnd;
		}
		clang::SourceLocation getEntireCodeStart() {
			return entireCodeStart;
		}
		clang::SourceLocation getEntireCodeEnd() {
			return entireCodeEnd;
		}
		clang::FunctionDecl* getFunc() {
			return func;
		}
		size_t getNumArgs() {
			return func->getNumParams();
		}
		std::string getArgName(unsigned i) {
			clang::ParmVarDecl* pvd = func->getParamDecl(i);
			return pvd->getName();
		}
		bool isArgParam(unsigned i) {
			std::string argName = getArgName(i);
			for (unsigned j = 0; j < getNumKnobs(); ++j) {
				if (!argName.compare(getKnobName(j))) {
					return true;
				}
			}
			return false;
		}
		bool isArgIn(unsigned i) {
			std::string argName = getArgName(i);
			for (unsigned j = 0; j < getNumKnobs(); ++j) {
				if (!argName.compare(getInName(j))) {
					return true;
				}
			}
			return false;
		}
		bool isArgOut(unsigned i) {
			std::string argName = getArgName(i);
			for (unsigned j = 0; j < getNumKnobs(); ++j) {
				if (!argName.compare(getOutName(j))) {
					return true;
				}
			}
			return false;
		}
		int getWARIdx(std::string name) {
			for (unsigned i = 0; i < getNumWARs(); ++i) {
				if (!name.compare(getWARName(i))) {
					return (int)i;
				}
			}
			return -1;
		}
		int getInIdx(std::string name) {
			for (unsigned i = 0; i < getNumIns(); ++i) {
				if (!name.compare(getInName(i))) {
					return (int)i;
				}
			}
			return -1;
		}
		int getOutIdx(std::string name) {
			for (unsigned i = 0; i < getNumOuts(); ++i) {
				if (!name.compare(getOutName(i))) {
					return (int)i;
				}
			}
			return -1;
		}
		bool isArgInOut(unsigned i) {
			return isArgIn(i) || isArgOut(i);
		}
		std::string getArgTypeStr(unsigned i) {
			clang::ParmVarDecl* pvd = func->getParamDecl(i);
			return pvd->getOriginalType().getAsString();
		}
		std::string getInLogSize(unsigned i) {
			return ins[i].second.second;
		}
		std::string getOutLogSize(unsigned i) {
			return outs[i].second.second;
		}
		std::string getWARLogSize(unsigned i) {
			return inouts[i].second.second;
		}
		std::string getWARTypeStr(unsigned i) {
			return inouts[i].second.first.getAsString();
		}
		std::string getOutTypeStr(unsigned i) {
			return outs[i].second.first.getAsString();
		}
		std::string getInTypeStr(unsigned i) {
			return ins[i].second.first.getAsString();
		}
		std::string getWARBakName(unsigned i) {
			return "_jit_bak_" + std::to_string(id) + "_" + std::to_string(i);
		}
		std::string getWARName(unsigned i) {
			return inouts[i].first;
		}
		std::string getInBakName(unsigned i) {
			return "_jit_bak_" + std::to_string(id) + "_" + std::to_string(i);
		}
		std::string getInName(unsigned i) {
			return ins[i].first;
		}
		std::string getOutName(unsigned i) {
			return outs[i].first;
		}
		std::string getKnobMin(unsigned i) {
			return knobs[i].second;
		}
		int getKnobIdx(std::string name) {
			for (unsigned i = 0; i < getNumKnobs(); ++i) {
				if (!name.compare(getKnobName(i))) {
					return (int)i;
				}
			}
			return -1;
		}
		std::string getKnobTypeStr(unsigned i) {
			clang::DeclRefExpr* dre = clang::cast<clang::DeclRefExpr>(knobs[i].first->IgnoreImplicit());
			clang::ValueDecl* decl = dre->getDecl();
			return decl->getType().getAsString();
		}
		std::string getKnobName(unsigned i) {
			clang::DeclRefExpr* dre = clang::cast<clang::DeclRefExpr>(knobs[i].first->IgnoreImplicit());
			clang::NamedDecl* nd = dre->getFoundDecl();
			return nd->getName();
		}
		std::string getKnobBound(unsigned i) {
			return "_jit_" + std::to_string(id) + "_" + getKnobName(i);
		}
		std::string getSafeFuncName() {
			return "_jit_safe_" + std::to_string(id);
		}
		std::string getFallbackFlagName() {
			return "_jit_disabled_" + std::to_string(id);
		}
		std::string getWrapperFuncName() {
			return "_jit_safe" + std::to_string(id) + "_wrapper";
			//return getFuncName() + "_wrapper";
		}
		std::string getFuncName() {
			return func->getNameAsString();
		}
		bool isRecursive() {
			return getNumKnobs() > 0;
		}

	private: 
		unsigned id;
		clang::SourceLocation atomicCodeStart;
		clang::SourceLocation atomicCodeEnd;
		clang::SourceLocation elseCodeStart;
		clang::SourceLocation elseCodeEnd;
		clang::SourceLocation entireCodeStart;
		clang::SourceLocation entireCodeEnd;
		clang::FunctionDecl* func;
		knob_vec knobs;
		inout_vec ins;
		inout_vec outs;
		inout_vec inouts;
		std::string fallback;
};


#endif
