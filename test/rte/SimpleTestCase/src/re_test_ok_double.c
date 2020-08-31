void Enter_test(void) __attribute__((used));
void Exit_test(void) __attribute__((used));

void re_test_ok_double() {
    Enter_test();
    Exit_test();
    Enter_test();
    Exit_test();
}
