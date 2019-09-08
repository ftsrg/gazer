//@expect fail
//@flag -bound 1
//@flag -inline-globals
//@flag -math-int

int a, b = 5;
void __VERIFIER_error();
int main() {
  while (1) {
    if (b == 5 || b == 6)
      b = a = 4;
    if (a)
      __VERIFIER_error();
  }
}
