#ifndef __JIT_FRONTEND__
#define __JIT_FRONTEND__

#include "global.h"
#include "function.h"
#include "stringMaker.h"
#include <vector>
#include <fstream>

#define MEAS_GPIO "P1"
#define MEAS_BIT "BIT3"

// non-class function
std::string getKnobVal(const std::string& str);
void insertString(std::string newStr, clang::SourceLocation insertAt, clang::Rewriter& R, bool insertAfter);
void insertString(std::string newStr, clang::Stmt* insertAt, clang::Rewriter& R, bool insertAfter);
void replaceStmt(clang::Stmt* st, std::string newStr, clang::Rewriter& R);
void removeStmt(clang::Stmt* st, clang::Rewriter& R);
std::string getStmtAsStr(clang::Stmt* st, clang::Rewriter& R);
clang::SourceRange getStmtRange(clang::Stmt* st, clang::Rewriter& R);

class MyASTVisitor : public clang::RecursiveASTVisitor<MyASTVisitor> {
	public:
		MyASTVisitor(clang::Rewriter &R) : TheRewriter {R},
			curF {nullptr}, counter {0}, curFDecl {nullptr}, isMeasureInitInserted {false}, isInMain {false}, mainBeginLoc {clang::SourceLocation::getFromRawEncoding(0)} {}

		bool VisitDecl(clang::Decl *decl);
		bool VisitStmt(clang::Stmt *s);
		void handleAtomicBegin(clang::CallExpr* ce);
		void handleElse(clang::CallExpr* ce);
		void handleEndIf(clang::CallExpr* ce);
		void handleKnob(clang::CallExpr* ce);
		void handleFallback(clang::CallExpr* ce);
		void handleIn(clang::CallExpr* ce);
		void handleOut(clang::CallExpr* ce);
		void handleInOut(clang::CallExpr* ce);
		void transformCode();
		void transformRecursiveCode();
		void insertMeasureCode();
		void insertMeasureDecl(clang::FunctionDecl* F);
		void insertMeasureInit();
		void createSafeFunc();
		void createWrapperFunc();
		clang::Rewriter& getRewriter() {
			return TheRewriter;
		}

	private:
		clang::Rewriter &TheRewriter;
		ProtectedFunction* curF;
		unsigned counter;
		clang::FunctionDecl* curFDecl;
		bool isMeasureInitInserted;
		bool isInMain;
		clang::SourceLocation mainBeginLoc;
		std::vector<std::string> measureCodes;
};


#endif
