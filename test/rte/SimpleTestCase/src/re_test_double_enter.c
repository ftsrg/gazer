void Enter_test(void) __attribute__((used));
void Exit_test(void) __attribute__((used));

void re_test_double_enter() {
    Enter_test();
    Enter_test();
    Exit_test();
    Exit_test();
}
