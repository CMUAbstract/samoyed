#include <stdio.h>
int _jit_safe_0()  {
	;
	printf();
	return 0;
}
void safe_printf(...) {
	PROTECT_BEGIN();
	_jit_safe_0();
	PROTECT_END();
	;
}
__nv int _jit_1_b = 10000;
int _jit_safe_1(int * a, int b)  {
	if (b <= _jit_1_b && !_jit_no_progress) {
		;
		for (unsigned i = 0; i < b; ++i) {
			*(a+i)++;
		}
		_jit_1_b = b;
		return 1;
	}
	return 0;
}
void test(int* a, int b) {
	int success = 0;
	PROTECT_BEGIN();
	success = _jit_safe_1(a, b);
	PROTECT_END();
	if (!success) {
		;
		test(a, b >> 1);
		test(a + (b >> 1), b >> 1);
	}
	;
}
/*
	 void safe_printf(...) {
	 IF_ATOMIC();
	 printf();
	 END_IF();
	 }
	 void test(int* a, int b) {
	 IF_ATOMIC(KNOB(b, 10000));
	 for (unsigned i = 0; i < b; ++i) {
 *(a+i)++;
 }
 ELSE();
 test(a, b >> 1);
 test(a + (b >> 1), b >> 1);
 END_IF();
 }
 */

int main() {
	int a[30];
	int b;
	int n;

	test(a, n);
	return 0;
}
//SAFE_BEGIN();
//printf("test %u", b);
//SAFE_END();
//SAFE_BEGIN();
//test(a, KNOB(b));
//UPDATE(0, ARG()+KNOB());
//SAFE_END();
//	SAFE_BEGIN();
//	test(a, KNOB0(b));
//	SAFE_IF_FAIL();
//	test(ARG(0), KNOB0()/2);
//	test(ARG(0)+KNOB0()/2, KNOB0()/2);
//	SAFE_END();
