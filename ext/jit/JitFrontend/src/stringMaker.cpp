#include "include/stringMaker.h"

std::string StringMaker::getDecl(std::string type, std::string argName, std::string initVal)
{
	std::string buf = "";
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	buf += type + " " + argName;
	if (!initVal.empty()) {
		buf += " = " + initVal;
	}
	buf += ";\n";

	return buf;
}

std::string StringMaker::getAssign(std::string lv, std::string rv)
{
	std::string buf = "";
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	buf += lv + " = " + rv + ";\n";

	return buf;
}

std::string StringMaker::getCall(std::string func, std::vector<std::string>& args, std::string ret)
{
	std::string buf = "";
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	
	if (!ret.empty()) {
		buf += ret + " = ";
	}

	buf += func + "(";
	for (unsigned i = 0; i < args.size(); ++i) {
		buf += args[i];
		if (i != args.size() - 1)
			buf += ", ";
	}
	buf += ");\n";

	return buf;
}

std::string StringMaker::getBrakStart(std::string name, std::string cond)
{
	std::string buf = "";
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	buf += name;
	if (!cond.empty()) {
		buf += " (" + cond + ")";
	}
	buf += " {\n";

	tabNum++;
	return buf;
}

std::string StringMaker::getStr(std::string str)
{
	std::string buf = "";
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	buf += str + ";\n";

	return buf;
}

std::string StringMaker::getWhileStart(std::string cond)
{
	return getBrakStart("while", cond);
}

std::string StringMaker::getElseStart()
{
	std::string buf = "";
	buf = getBrakEnd();

	return buf + getBrakStart("else", "");
}

std::string StringMaker::getIfStart(std::string cond)
{
	return getBrakStart("if", cond);
}

std::string StringMaker::getBrakEnd()
{
	std::string buf = "";
	tabNum--;
	for (unsigned i = 0; i < tabNum; ++i)
		buf += "\t";
	buf += "}\n";

	return buf;
}
