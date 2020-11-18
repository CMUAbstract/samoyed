#ifndef __STRINGMAKER__
#define __STRINGMAKER__

#include "global.h"
#include <vector>

class StringMaker {
	public:
		StringMaker(unsigned _t): tabNum {_t} {}
		StringMaker(): StringMaker(0) {}
		std::string getAssign(std::string lv, std::string rv);
		std::string getDecl(std::string type, std::string argName, std::string initVal = "");
		std::string getBrakStart(std::string name, std::string cond);
		std::string getWhileStart(std::string cond);
		std::string getIfStart(std::string cond);
		std::string getElseStart();
		std::string getBrakEnd();
		std::string getStr(std::string str);
		std::string getCall(std::string func, std::vector<std::string>& args, std::string ret = "");
		std::string getPBegin() {
			std::vector<std::string> args;
			return getCall("PROTECT_BEGIN", args);
		}
		std::string getPEnd() {
			std::vector<std::string> args;
			return getCall("PROTECT_END", args);
		}
		std::string getPEndNoWARClr() {
			std::vector<std::string> args;
			return getCall("PROTECT_END_NOWARCLR", args);
		}
		void setTabNum(unsigned _t) {
			tabNum = _t;
		}
		std::string getMeasureBegin(std::string comp, std::string val) {
			return ("#if ENERGY " + comp + " " + val + "\n");
		}
		std::string getMeasureElse() {
			return ("#else\n");
		}
		std::string getMeasureEnd() {
			return ("#endif\n");
		}
		std::string setGPIO(std::string gpio, std::string bit) {
			return getStr(gpio + " |= " + bit);
		}
		std::string clrGPIO(std::string gpio, std::string bit) {
			return getStr(gpio + " &= ~" + bit);
		}
		std::string setGPIOOut(std::string gpio, std::string bit) {
			return setGPIO(gpio + "OUT", bit);
		}
		std::string clrGPIOOut(std::string gpio, std::string bit) {
			return clrGPIO(gpio + "OUT", bit);
		}
		std::string setGPIODir(std::string gpio, std::string bit) {
			return setGPIO(gpio + "DIR", bit);
		}

	private:
		unsigned tabNum;
};


#endif
