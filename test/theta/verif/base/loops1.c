// RUN: %theta -memory=havoc -math-int "%s" | FileCheck "%s"
// RUN: %theta --domain EXPL --refinement UNSAT_CORE "%s" | FileCheck "%s"


// CHECK: Verification SUCCESSFUL
extern int __VERIFIER_nondet_int(void);

int calculate(int x)
{
    int aggr = 0, aggr2 = x;
    int i = 0, j = 0;

    while (i < x) {
        aggr = aggr + i;
        while (j < x / 2) {
            aggr = aggr * (j + 1);
            aggr2 = aggr2 - 1;
        }
    }

    return aggr + aggr2;
}

int main(void)
{
    int i = 0;
    int c = __VERIFIER_nondet_int();

    while (i < c) {
        calculate(i);
        ++i;
    }
}
