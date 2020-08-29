// RUN: %bmc -bound 10 -trace "%s" | FileCheck "%s"

// CHECK: Verification FAILED

extern void __VERIFIER_error() __attribute__ ((__noreturn__));
extern int __VERIFIER_nondet_int();

int main()
{
    int p13 = __VERIFIER_nondet_int();
    int lk13;

    // CHECK: call __VERIFIER_nondet_int() returned #0bv32 at [[# @LINE + 2]]:15
    // CHECK: p14 := 0 {{\([a-zA-Z0-9\s]*\)}} at [[# @LINE + 1]]:9
    int p14 = __VERIFIER_nondet_int();
    int lk14;

    int cond;

    while(1) {
        cond = __VERIFIER_nondet_int();
        if (cond == 0) {
            goto out;
        } else {}

        lk13 = 0;  // CHECK: lk13 := 0 {{\([a-zA-Z0-9\s]*\)}} at [[# @LINE]]:14
        lk14 = 0;  // CHECK: lk14 := 0 {{\([a-zA-Z0-9\s]*\)}} at [[# @LINE]]:14

        if (p13 != 0) {
            lk13 = 1;
        } else {}

        if (p14 != 0) {
            lk14 = 1;
        } else {}

        if (p13 != 0) {
            if (lk13 != 1) goto ERROR;
            lk13 = 0;
        } else {}

        if (p14 != 0) {
            if (lk14 != 1) goto ERROR;
            lk14 = 0;
        } else {goto ERROR;}

    }
  out:
    return 0;
  ERROR: __VERIFIER_error();
    return 0;
}

