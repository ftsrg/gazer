//@expect success
//@flag -bound 10
b(c) {   
  if (!c)
    __VERIFIER_error();
}

main() {
  int a = d();
  char e, f;
  short carry;
  e = a;
  carry = f = 0;
  while (f < 4 || carry) {
    if (e)
      carry = 1;
    f = f + 1;
  }
  b(a + 70789);
}
