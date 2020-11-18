//------------------------------------------------------------------------------
// Original code base was from
// Eli Bendersky (eliben@gmail.com)
//------------------------------------------------------------------------------
#include "include/jit_frontend.h"
#include <regex>

using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;

static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");

std::string replaceVar(const std::string& str, const std::string& var0, const std::string& var1) {
	llvm::errs() << "error -1\n";
	std::smatch m;
	// check starting with var0
	std::regex e ("^"+var0+"[^a-z_]?");
	std::string result(str);
	std::regex_search(result, m, e);

	llvm::errs() << "error 0\n";

	while (m.size() > 0) {
		result.replace(m.position(0), var0.length(), var1);
		std::regex_search(result, m, e);
	}

	llvm::errs() << "error 1\n";
	// next, find var0 in the middle
	std::regex e2 ("[^a-z_]"+var0+"[^a-z_]?");
	std::regex_search(result, m, e2);

	llvm::errs() << "error 2\n";
	while (m.size() > 0) {
		result.replace(m.position(0) + 1, var0.length(), var1);
		std::regex_search(result, m, e2);
	}

	llvm::errs() << result << "\n";

	return result;
}


// TODO: stupid impl. Possible edge cases that I do not consider
std::string getInoutVal(const std::string& str, const std::string& key) {
	std::size_t pos = str.find(key + "(");
	assert(pos != std::string::npos);
	std::string newStr = str.substr(pos + strlen((key + "(").c_str()));

	pos = newStr.find(",");
	if (pos == std::string::npos)
		pos = newStr.find(")");
	assert(pos != std::string::npos);
	newStr = newStr.substr(0, pos); // drop "," or ")"
	return newStr;
}
std::string getInVal(const std::string& str) {
	return getInoutVal(str, "IN");
}
std::string getOutVal(const std::string& str) {
	return getInoutVal(str, "OUT");
}
std::string getWARVal(const std::string& str) {
	return getInoutVal(str, "INOUT");
}
std::string getFallbackVal(const std::string& str) {
	// TODO: We can merge this to getInoutVal, but I am too lazy for that.
	std::string key = "FALLBACK";
	std::size_t pos = str.find(key + "(");
	assert(pos != std::string::npos);
	std::string newStr = str.substr(pos + strlen((key + "(").c_str()));

	pos = newStr.find(")");
	assert(pos != std::string::npos);
	newStr = newStr.substr(0, pos); // drop "," or ")"
	return newStr;
}

// TODO: stupid impl. Possible edge cases that I do not consider
std::string getInoutSize(const std::string& str, const std::string& key) {
	std::size_t pos = str.find(key + "(");
	assert(pos != std::string::npos);
	pos = str.find(",");
	std::string newStr = str.substr(pos + strlen(","));

	pos = newStr.find(",");
	assert(pos == std::string::npos);
	pos = newStr.find(")");
	assert(pos != std::string::npos);
	newStr = newStr.substr(0, pos); // drop  ")"
	return newStr;
}
std::string getInSize(const std::string& str) {
	return getInoutSize(str, "IN");
}
std::string getOutSize(const std::string& str) {
	return getInoutSize(str, "OUT");
}
std::string getWARSize(const std::string& str) {
	return getInoutSize(str, "INOUT");
}

void printType(QualType* ty) {
	llvm::errs() << ty->getAsString() << "\n";
}

void printStmt(Stmt* st) {
	LangOptions LangOpts;
	LangOpts.CPlusPlus = true;
	clang::PrintingPolicy Policy(LangOpts);
	st->printPretty(llvm::errs() , NULL, Policy);
	llvm::errs() << "\n";
}

void insertString(std::string newStr, SourceLocation insertAt, Rewriter& R, bool insertAfter) {
	R.InsertText(insertAt, newStr, insertAfter, true);
}

void insertString(std::string newStr, Stmt* insertAt, Rewriter& R, bool insertAfter) {
	R.InsertText(getStmtRange(insertAt, R).getBegin(), newStr, insertAfter, true);
}

void replaceStmt(Stmt* st, std::string newStr, Rewriter& R) {
	R.ReplaceText(getStmtRange(st, R), newStr);
}

void removeStmt(Stmt* st, Rewriter& R) {
	R.RemoveText(getStmtRange(st, R));
}

std::string getStmtAsStr(Stmt* st, Rewriter& R) {
	return R.getRewrittenText(getStmtRange(st, R));
}

SourceRange getStmtRange(Stmt* st, Rewriter& R) {
	SourceLocation startLoc = st->getBeginLoc();
	SourceLocation endLoc = st->getEndLoc();

	while ( startLoc.isMacroID() ) {
		llvm::errs() << "is macro!!\n";
		// Get the start/end expansion locations
		//std::pair< SourceLocation, SourceLocation > expansionRange = 
		CharSourceRange expansionRange = 
			R.getSourceMgr().getImmediateExpansionRange( startLoc );

		// We're just interested in the start location
		startLoc = expansionRange.getBegin();
		endLoc = expansionRange.getEnd();
		//llvm::errs() << startLoc.getRawEncoding() << "\n";
	}

	SourceRange expandedLoc( startLoc, endLoc );
	return expandedLoc;
}

void MyASTVisitor::insertMeasureDecl(FunctionDecl* f) {
	StringMaker sm(0);

	std::string newCode = "";
	newCode += sm.getMeasureBegin(">", "0"); // every atomic region share this
	newCode += sm.getDecl("__nv uint16_t",
			"energy_counter", "0");
	newCode += sm.getDecl("__nv uint16_t",
			"energy_overflow", "0");
	newCode += sm.getMeasureEnd();

	// Declare vars for energy measurement
	insertString(newCode,
		f->getSourceRange().getBegin(),
			TheRewriter, false);


	// not sure why, but this gets called multiple times
	// so prevent it
	isMeasureInitInserted = true;
}

void MyASTVisitor::insertMeasureInit() {
	StringMaker sm(0);

	std::string newCode = "";
	// Add energy measuring code, including the init codes which is currently inserted manually
	newCode += sm.getAssign("chkpt_mask_init", "1");
	std::vector<std::string> args;
	newCode += sm.getCall("init", args);
	newCode += sm.getMeasureBegin("==", "0"); // every atomic region share this
	newCode += sm.getCall("restore_regs", args);
	newCode += sm.getMeasureEnd();
	newCode += sm.getAssign("chkpt_mask_init", "0");

	insertString(newCode,
			mainBeginLoc,
			TheRewriter, true);
}

void MyASTVisitor::insertMeasureCode() {
	StringMaker sm(0);
	std::string newCode = "";

	newCode += sm.getMeasureBegin("==", std::to_string(counter)); // different per atomic region

	// init
	newCode += sm.clrGPIOOut(MEAS_GPIO, MEAS_BIT);
	newCode += sm.setGPIODir(MEAS_GPIO, MEAS_BIT);

	llvm::errs() << "debug 1\n";
	std::vector<std::string> args;
	// Decl temp var for in-out
	for (unsigned i = 0; i < curF->getNumIns(); ++i) {
		llvm::errs() << "For in " << i << "\n";
		if (curF->getInLogSize(i).size() == 0) {
			llvm::errs() << "Is scalar\n";
			newCode += sm.getDecl(curF->getInTypeStr(i), curF->getInName(i));
		} else {
			llvm::errs() << "Is array\n";
			std::string logSize = curF->getInLogSize(i);
			llvm::errs() << "Log size: " << logSize << "\n";
			// If IN size contains PARAM, replace the string to PARAM min val
			for (unsigned j = 0; j < curF->getNumKnobs(); ++j) {
				logSize = replaceVar(logSize, curF->getKnobName(j), curF->getKnobMin(j));
			}
			llvm::errs() << "Replacing end\n";
			// if array, always assign char array
			// and do typecast later
			newCode += sm.getDecl("char", "_jit_test_" + curF->getInName(i) + "[" + logSize + "]");
			//newCode += sm.getDecl(curF->getInTypeStr(i), "_jit_test_" + curF->getInName(i),
			//		"malloc(" + logSize + ")");
			llvm::errs() << "add random\n";

			// Randomize!
			args.clear();
			args.push_back("_jit_test_" + curF->getInName(i));
			args.push_back(logSize);
			newCode += sm.getCall("fill_with_rand", args);
		}
	}
	for (unsigned i = 0; i < curF->getNumWARs(); ++i) {
		llvm::errs() << "For inout " << i << "\n";
		if (curF->getWARLogSize(i).size() == 0) {
			llvm::errs() << "Is scalar\n";
			newCode += sm.getDecl(curF->getWARTypeStr(i), curF->getWARName(i));
		} else {
			llvm::errs() << "Is array\n";
			std::string logSize = curF->getWARLogSize(i);
			llvm::errs() << "Log size: " << logSize << "\n";
			// If IN size contains PARAM, replace the string to PARAM min val
			for (unsigned j = 0; j < curF->getNumKnobs(); ++j) {
				logSize = replaceVar(logSize, curF->getKnobName(j), curF->getKnobMin(j));
			}
			llvm::errs() << "Replacing end\n";
			// if array, always assign char array
			// and do typecast later
			newCode += sm.getDecl("char", "_jit_test_" + curF->getWARName(i) + "[" + logSize + "]");
			//newCode += sm.getDecl(curF->getInTypeStr(i), "_jit_test_" + curF->getInName(i),
			//		"malloc(" + logSize + ")");
			llvm::errs() << "add random\n";

			// Randomize!
			args.clear();
			args.push_back("_jit_test_" + curF->getWARName(i));
			args.push_back(logSize);
			newCode += sm.getCall("fill_with_rand", args);
		}
	}
	llvm::errs() << "debug 2\n";
	for (unsigned i = 0; i < curF->getNumOuts(); ++i) {
		if (curF->getOutLogSize(i).size() == 0) {
			newCode += sm.getDecl(curF->getOutTypeStr(i), curF->getOutName(i));
		} else {
			std::string logSize = curF->getOutLogSize(i);
			// If IN size contains PARAM, replace the string to PARAM min val
			for (unsigned j = 0; j < curF->getNumKnobs(); ++j) {
				logSize = replaceVar(logSize, curF->getKnobName(j), curF->getKnobMin(j));
			}
			// if out and in overlaps, don't do this
			if (curF->getInIdx(curF->getOutName(i)) == -1) {
				newCode += sm.getDecl("char", "_jit_test_" + curF->getOutName(i) + "[" + logSize + "]");
			}
		}
	}
	llvm::errs() << "debug 3\n";

	args.clear();
	args.push_back("\"CNT: %u %u\\r\\n\"");
	args.push_back("energy_overflow");
	args.push_back("energy_counter");
	newCode += sm.getCall("PRINTF", args);
	newCode += sm.getAssign("energy_counter", "0");
	newCode += sm.getAssign("energy_overflow", "0");
	newCode += sm.setGPIOOut(MEAS_GPIO, MEAS_BIT);

	// actually run the test
	newCode += sm.getWhileStart("1");
	args.clear();
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		int idx;
		if ((idx = curF->getKnobIdx(curF->getArgName(i))) != -1) {
			args.push_back(curF->getKnobMin(idx));
		}
		else if ((idx = curF->getInIdx(curF->getArgName(i))) != -1) {
			// do typecast!
			args.push_back("(" + curF->getInTypeStr(idx) + ")_jit_test_" + curF->getArgName(i));
		}
		else if ((idx = curF->getOutIdx(curF->getArgName(i))) != -1) {
			// do typecast!
			args.push_back("(" + curF->getOutTypeStr(idx) + ")_jit_test_" + curF->getArgName(i));
		}
		else if ((idx = curF->getWARIdx(curF->getArgName(i))) != -1) {
			// do typecast!
			args.push_back("(" + curF->getWARTypeStr(idx) + ")_jit_test_" + curF->getArgName(i));
		}
		else {
			assert(false && "What case can this be?");
		}
	}
	llvm::errs() << "debug 4\n";
	newCode += sm.getCall(curF->getFunc()->getNameAsString(), args);
	newCode += sm.getIfStart("energy_counter == 0xFFFF");
	newCode += sm.getAssign("energy_overflow", "1");
	newCode += sm.getElseStart();
	newCode += sm.getAssign("energy_counter", "energy_counter + 1");
	newCode += sm.getBrakEnd();
	newCode += sm.getBrakEnd();

	newCode += sm.getMeasureEnd();

	if (mainBeginLoc == clang::SourceLocation::getFromRawEncoding(0)) {
		llvm::errs() << "Saved for later\n";
		llvm::errs() << newCode << "\n";
		measureCodes.push_back(newCode);
	}
	else {
		// TODO: current impl bug
		assert(false && "Main should come last!");
		//		llvm::errs() << "Insert!\n";
		//		insertString(newCode,
		//				mainBeginLoc,
		//				TheRewriter, true);
	}
}

bool MyASTVisitor::VisitDecl(Decl *decl) {
	if (decl->isFunctionOrFunctionTemplate()) {
		curFDecl = decl->getAsFunction();
		std::string funcName = curFDecl->getNameAsString();
		llvm::errs() << "Func: " << funcName << "\n";
		if (!isMeasureInitInserted && !funcName.compare("main")) {
			isInMain = true;
		}
	}
	return true;
}

bool MyASTVisitor::VisitStmt(Stmt *s) {
	//printStmt(s);
	// Insert init functions in main
	if (isInMain && !isa<CompoundStmt>(s)) {
		llvm::errs() << "First code in main\n";
		printStmt(s);
		// insert before the first code
		mainBeginLoc = getStmtRange(s, TheRewriter).getBegin();
		if (measureCodes.size()) {
			//insertMeasureDecl(curFDecl);
			insertMeasureInit();
		}
		// flush measure codes made before reaching main
		for (std::vector<std::string>::iterator VI = measureCodes.begin(), VE = measureCodes.end();
				VI != VE; ++VI) {
			llvm::errs() << "Insert!!\n";
			llvm::errs() << *VI << "\n";
			insertString(*VI,
					mainBeginLoc,
					TheRewriter, true);

		}
		isInMain = false;
	}
	// Visit call instructions
	if (CallExpr* ce = dyn_cast<CallExpr>(s)) {
		std::string funcName {ce->getDirectCallee()->getName()};

		if (funcName.find("IF_ATOMIC") != std::string::npos) {
			handleAtomicBegin(ce);
		}
		else if (funcName.find("ELSE") != std::string::npos) {
			handleElse(ce);
		}
		else if (funcName.find("END_IF") != std::string::npos) {
			handleEndIf(ce);
		}
		else if (funcName.find("PARAM") != std::string::npos) {
			handleKnob(ce);
		}
		else if (funcName.find("INOUT") != std::string::npos) {
			handleInOut(ce);
		}
		else if (funcName.find("IN") != std::string::npos) {
			handleIn(ce);
		}
		else if (funcName.find("OUT") != std::string::npos) {
			handleOut(ce);
		}
		else if (funcName.find("FALLBACK") != std::string::npos) {
			handleFallback(ce);
		}
		//else if (funcName.find("WAR") != std::string::npos) {
		//	handleWAR(ce);
		//}
	}
	return true;
}

void MyASTVisitor::handleAtomicBegin(CallExpr* ce) {
	assert(!curF && "Previous function has not seen END_IF");
	llvm::errs() << "begin\n";
	curF = new ProtectedFunction(counter, curFDecl);
	SourceLocation atomicCodeStart = getStmtRange(ce, TheRewriter).getEnd();
	atomicCodeStart = atomicCodeStart.getLocWithOffset(1);
	curF->setAtomicCodeStart(atomicCodeStart);
	SourceLocation entireCodeStart = getStmtRange(ce, TheRewriter).getBegin();
	curF->setEntireCodeStart(entireCodeStart);
}

void MyASTVisitor::handleIn(CallExpr* ce) {
	assert(curF && "In without IF_ATOMIC");
	assert(ce->getNumArgs() <= 2 && "In only takes one or two arg");

	Expr* val = ce->getArg(0)->IgnoreImplicit();
	QualType ty = val->getType();
	std::string name = getInVal(getStmtAsStr(ce, TheRewriter));

	if (ce->getNumArgs() == 1) {
		curF->addIns(name, ty);
	}
	else {
		// TODO: this should only take even.
		// How can we check?
		std::string size = getInSize(getStmtAsStr(ce, TheRewriter));
		curF->addIns(name, ty, size);
	}
	removeStmt(ce, TheRewriter);
}

void MyASTVisitor::handleOut(CallExpr* ce) {
	assert(curF && "Out without IF_ATOMIC");
	assert(ce->getNumArgs() <= 2 && "Out only takes one or two arg");

	Expr* val = ce->getArg(0)->IgnoreImplicit();
	QualType ty = val->getType();
	std::string name = getOutVal(getStmtAsStr(ce, TheRewriter));

	if (ce->getNumArgs() == 1) {
		curF->addOuts(name, ty);
	}
	else {
		// TODO: this should only take even.
		// How can we check?
		std::string size = getOutSize(getStmtAsStr(ce, TheRewriter));
		curF->addOuts(name, ty, size);
	}
	removeStmt(ce, TheRewriter);
}

void MyASTVisitor::handleInOut(CallExpr* ce) {
	assert(curF && "InOut without IF_ATOMIC");
	assert(ce->getNumArgs() <= 2 && "InOut only takes one or two arg");

	Expr* val = ce->getArg(0)->IgnoreImplicit();
	QualType ty = val->getType();
	std::string name = getWARVal(getStmtAsStr(ce, TheRewriter));

	if (ce->getNumArgs() == 1) {
		curF->addInOuts(name, ty);
	}
	else {
		// TODO: this should only take even.
		// How can we check?
		std::string size = getWARSize(getStmtAsStr(ce, TheRewriter));
		curF->addInOuts(name, ty, size);
	}
	removeStmt(ce, TheRewriter);
}

void MyASTVisitor::handleFallback(CallExpr* ce) {
	// For now, let's just allow one func for fallback
	assert(curF && "FALLBACK without IF_ATOMIC");
	assert(ce->getNumArgs() == 1 && "FALLBACK only takes one arg");
	llvm::errs() << "fallback\n";

	std::string name = getFallbackVal(getStmtAsStr(ce, TheRewriter));
	llvm::errs() << name << "\n";
	curF->addFallback(name + ")");

	removeStmt(ce, TheRewriter);
}

void MyASTVisitor::handleKnob(CallExpr* ce) {
	assert(curF && "KNOB without IF_ATOMIC");
	// one arg - name, defalut param size 1
	// two arg - name, min
	assert(ce->getNumArgs() <= 2 && "KNOB only takes one - two arg");
	llvm::errs() << "knob\n";

	Expr* knob = ce->getArg(0);
	if (ce->getNumArgs() == 1) {
		curF->addKnob(knob);
	}
	else {
		Expr* initMin = ce->getArg(1);
		llvm::APSInt min = initMin->EvaluateKnownConstInt(curF->getFunc()->getASTContext());
		curF->addKnob(knob, min.toString(10));
	}

	removeStmt(ce, TheRewriter);
}

void MyASTVisitor::handleElse(CallExpr* ce) {
	assert(curF->getNumKnobs() && "You need knobs for ELSE");
	assert(!curF->isAtomicCodeEndSet() && "ELSE or END_IF was already called!!");

	llvm::errs() << "else\n";
	SourceLocation atomicCodeEnd = getStmtRange(ce, TheRewriter).getBegin();
	curF->setAtomicCodeEnd(atomicCodeEnd.getLocWithOffset(-1));
	SourceLocation elseCodeStart = getStmtRange(ce, TheRewriter).getEnd();
	curF->setElseCodeStart(elseCodeStart.getLocWithOffset(1));
}

void MyASTVisitor::handleEndIf(CallExpr* ce) {
	assert(curF && "END_IF without IF_ATOMIC");
	llvm::errs() << "endif\n";

	SourceLocation entireCodeEnd = getStmtRange(ce, TheRewriter).getEnd();
	curF->setEntireCodeEnd(entireCodeEnd);
	if (curF->getNumKnobs()) {
		assert(curF->isElseCodeStartSet() && "ELSE should be called before");
		SourceLocation elseCodeEnd = getStmtRange(ce, TheRewriter).getBegin();
		curF->setElseCodeEnd(elseCodeEnd.getLocWithOffset(-1));
		transformRecursiveCode();
		llvm::errs() << "debug 0\n";
	}
	else {
		assert(!curF->isAtomicCodeEndSet() && "ELSE or END_IF was called before");
		SourceLocation atomicCodeEnd = getStmtRange(ce, TheRewriter).getBegin();
		atomicCodeEnd = atomicCodeEnd.getLocWithOffset(-1);
		curF->setAtomicCodeEnd(atomicCodeEnd);
		transformCode();
	}

	counter++;
	insertMeasureCode();
	llvm::errs() << "debug 9\n";
	delete curF;
	curF = nullptr;
}

void MyASTVisitor::transformCode() {
	// create a wrapper function for the safe func
	createWrapperFunc();
	// create new function first
	createSafeFunc();

	StringMaker sm(1);

	std::string newCode = "";
	// create if-else fallback
	newCode += sm.getIfStart("!" + curF->getFallbackFlagName());
	std::vector<std::string> args;
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		args.push_back(curF->getArgName(i));
	}
	newCode += sm.getCall(curF->getWrapperFuncName(), args);
	newCode += sm.getBrakEnd();

	newCode += sm.getIfStart(curF->getFallbackFlagName());
	newCode += sm.getStr(curF->getFallback());
	newCode += sm.getBrakEnd();

	TheRewriter.ReplaceText(SourceRange(curF->getEntireCodeStart(),
				curF->getEntireCodeEnd()), newCode);
}

void MyASTVisitor::transformRecursiveCode() {

	// create a wrapper function for the safe func
	createWrapperFunc();
	// create new function first
	createSafeFunc();

	StringMaker sm(1);

	std::string newCode = "";
	// create if-else fallback
	newCode += sm.getIfStart("!" + curF->getFallbackFlagName());
	std::vector<std::string> args;
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		args.push_back(curF->getArgName(i));
	}
	newCode += sm.getCall(curF->getWrapperFuncName(), args);
	newCode += sm.getBrakEnd();

	newCode += sm.getIfStart(curF->getFallbackFlagName());
	newCode += sm.getStr(curF->getFallback());
	newCode += sm.getBrakEnd();


	TheRewriter.ReplaceText(SourceRange(curF->getEntireCodeStart(),
				curF->getEntireCodeEnd()), newCode);
}

void MyASTVisitor::createWrapperFunc() {
	StringMaker sm(0);
	std::string funcDecl = "";

	// name the wrapper func
	funcDecl += "void " + curF->getWrapperFuncName();
	funcDecl += "(";

	// add arguments
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		funcDecl += curF->getArgTypeStr(i) + " ";
		funcDecl += curF->getArgName(i);
		if (i != curF->getNumArgs() - 1)
			funcDecl += ", ";
	}
	funcDecl += ") ";

	// function start
	funcDecl = sm.getBrakStart(funcDecl, "");

	// add sw fallback for recursive code here
	if (curF->isRecursive()) {
		std::string cond = "";
		for (unsigned i = 0; i < curF->getNumKnobs(); ++i) {
			cond += curF->getKnobName(i) + " < " + curF->getKnobMin(i);
			if (i != curF->getNumKnobs() - 1) {
				cond += " || ";
			}
		}
		funcDecl += sm.getIfStart(cond);
		funcDecl += sm.getAssign(curF->getFallbackFlagName(), "1");
		funcDecl += sm.getStr("return");
		funcDecl += sm.getBrakEnd();
	}

	if (curF->isRecursive()) {
		funcDecl += sm.getDecl("int", "success", "0");
	}
	size_t numWARs = curF->getNumWARs();
	bool undoLogCalled = false;
	// add undo-log if needed
	for (unsigned i = 0; i < numWARs; ++i) {
		std::string logSize = curF->getWARLogSize(i);
		if (logSize.size() == 0) {
			funcDecl += sm.getDecl(curF->getWARTypeStr(i),
					curF->getWARBakName(i), curF->getWARName(i));
		}
		else {
			// add if-else here that is same as the real if-else on the atomic region
			// so that when it is sure that we are not running the region,
			// do not undo-log as well
			// This optimization also applies for scalars, but we only do it for
			// arrays because it is expensive

			// add if only when needed (when there is knob)
			if (curF->isRecursive()) {
				// back up the knob

				std::string cond = "";
				for (unsigned i = 0; i < curF->getNumKnobs(); ++i) {
					cond += curF->getKnobName(i) + " <= " + curF->getKnobBound(i);
					if (i != curF->getNumKnobs() - 1)
						cond += " && ";
				}
				funcDecl += sm.getIfStart(cond);
			}
			std::vector<std::string> args;
			args.push_back(curF->getWARName(i));
			args.push_back(curF->getWARLogSize(i));
			funcDecl += sm.getCall("recursiveUndoLog", args);
			undoLogCalled = true;
			funcDecl += sm.getAssign("undoLogPtr", "undoLogPtr_tmp");
			funcDecl += sm.getAssign("undoLogCnt", "undoLogCnt_tmp");
			if (curF->isRecursive()) {
				funcDecl += sm.getBrakEnd();
			}
		}
	}

	funcDecl += sm.getPBegin();
	funcDecl += sm.getAssign("undoLogPtr", "undoLogPtr_tmp");
	funcDecl += sm.getAssign("undoLogCnt", "undoLogCnt_tmp");
	
	// if not recursive, fallback check here
	if (!curF->isRecursive()) {
		std::string cond = "_jit_no_progress";
		funcDecl += sm.getIfStart(cond);
		funcDecl += sm.getAssign(curF->getFallbackFlagName(), "1");
		funcDecl += sm.getPEnd();
		funcDecl += sm.getStr("return");
		funcDecl += sm.getBrakEnd();
	}

	// add undo-log if needed
	for (unsigned i = 0; i < numWARs; ++i) {
		std::string logSize = curF->getWARLogSize(i);
		if (logSize.size() == 0) {
			funcDecl += sm.getAssign(curF->getWARName(i),
					curF->getWARBakName(i));
		}
	}

	std::vector<std::string> args;
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		args.push_back(curF->getArgName(i));
	}
	if (curF->isRecursive()) {
		funcDecl += sm.getCall(curF->getSafeFuncName(), args, "success");
	} else {
		funcDecl += sm.getCall(curF->getSafeFuncName(), args);
	}
	funcDecl += sm.getPEnd();
	if (curF->isRecursive()) {
		funcDecl += sm.getIfStart("!success");

		funcDecl += replaceVar(curF->getElseStr(TheRewriter), curF->getFuncName(), curF->getWrapperFuncName());

		funcDecl += sm.getBrakEnd();
	}

	funcDecl += sm.getBrakEnd();
	llvm::errs() << "===t===\n" << funcDecl << "\n";

	insertString(
			funcDecl,
			curF->getFunc()->getSourceRange().getBegin(),
			TheRewriter, false);

	// add decl of the fallback flag
	insertString(
			sm.getDecl("__nv unsigned",
				curF->getFallbackFlagName(), "0"),
			curF->getFunc()->getSourceRange().getBegin(),
			TheRewriter, false);
}
void MyASTVisitor::createSafeFunc() {
	StringMaker sm(0);
	std::string funcDecl = "";

	// name the safe function
	funcDecl += "int " + curF->getSafeFuncName();
	funcDecl += "(";

	// add arguments
	for (unsigned i = 0; i < curF->getNumArgs(); ++i) {
		funcDecl += curF->getArgTypeStr(i) + " ";
		funcDecl += curF->getArgName(i);
		if (i != curF->getNumArgs() - 1)
			funcDecl += ", ";
	}
	funcDecl += ") ";

	// function start
	funcDecl = sm.getBrakStart(funcDecl, "");

	// add if only when needed (when there is knob)
	if (curF->getNumKnobs() > 0) {
		// back up the knob

		std::string cond = "";
		for (unsigned i = 0; i < curF->getNumKnobs(); ++i) {
			funcDecl += sm.getDecl(curF->getKnobTypeStr(i),
					curF->getKnobName(i) + "_bak",
					curF->getKnobName(i));
			cond += curF->getKnobName(i) + "_bak <= " + curF->getKnobBound(i);
			cond += " && ";
		}
		cond += "!_jit_no_progress";
		funcDecl += sm.getIfStart(cond);
	}

	// add the atomic region
	std::string atCode = curF->getAtomicStr(TheRewriter); 
	llvm::errs() << "Atomic code: " << atCode << "\n";

	funcDecl += atCode;

	// add rest of the peppering code only on knob
	if (curF->getNumKnobs() > 0) {
		// only update the bounds when no_progress ever happened
		// (to avoid updating bound when the workload itself was small)
		funcDecl += sm.getIfStart("_jit_bndMayNeedUpdate");
		for (unsigned i = 0; i < curF->getNumKnobs(); ++i) {
			funcDecl += sm.getAssign(curF->getKnobBound(i), curF->getKnobName(i) + "_bak");
		}
		funcDecl += sm.getAssign("_jit_bndMayNeedUpdate", "0");
		funcDecl += sm.getBrakEnd();

		funcDecl += sm.getStr("return 1");
		funcDecl += sm.getBrakEnd();
	}
	funcDecl += sm.getStr("return 0");

	funcDecl += sm.getBrakEnd();
	llvm::errs() << "===t===\n" << funcDecl << "\n";

	insertString(
			funcDecl,
			curF->getFunc()->getSourceRange().getBegin(),
			TheRewriter, false);

	// add decl of the knobBounds
	for (unsigned i = 0; i < curF->getNumKnobs(); ++i) {
		insertString(
				//sm.getDecl("__nv " + curF->getKnobTypeStr(i),
				//	curF->getKnobBound(i), curF->getKnobInitBnd(i)),
				// TODO: tmp. To be correct, max depending on the type should be used
				sm.getDecl("__nv " + curF->getKnobTypeStr(i),
					curF->getKnobBound(i), "65535"),
				curF->getFunc()->getSourceRange().getBegin(),
				TheRewriter, false);
	}
}

// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
	public:
		MyASTConsumer(Rewriter &R) : Visitor(R) {}

		// Override the method that gets called for each parsed top-level
		// declaration.
		bool HandleTopLevelDecl(DeclGroupRef DR) override {
			for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
				// Traverse the declaration using our AST visitor.
				llvm::errs() << "**** " << *b << " ****\n";
				Visitor.TraverseDecl(*b);
				(*b)->dump();
			}
			return true;
		}

	private:
		MyASTVisitor Visitor;
};

// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
	public:
		MyFrontendAction() {}
		void EndSourceFileAction() override {
			SourceManager &SM = TheRewriter.getSourceMgr();
			llvm::errs() << "** EndSourceFileAction for: "
				<< SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";

			// Now emit the rewritten buffer.
			TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());

			// emit to real file
			//TheRewriter.getEditBuffer(SM.getMainFileID()).overwriteChangedFiles();
			TheRewriter.overwriteChangedFiles();
		}

		std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
				StringRef file) override {
			llvm::errs() << "** Creating AST consumer for: " << file << "\n";
			TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
			return std::make_unique<MyASTConsumer>(TheRewriter);
		}

	private:
		Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
	llvm::errs() << "start\n";
	CommonOptionsParser op(argc, argv, ToolingSampleCategory);
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());

	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
