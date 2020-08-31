void Enter_test(void) __attribute__((used));
void Exit_test(void) __attribute__((used));

void re_test_no_enter() {
    Exit_test();
}
