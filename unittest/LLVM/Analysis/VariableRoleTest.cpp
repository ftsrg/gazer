#include "gazer/LLVM/Analysis/VariableRoles.h"

#include <llvm/IR/LLVMContext.h>
#include <llvm/AsmParser/Parser.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/Module.h>

#include <gtest/gtest.h>

using namespace gazer;


class VariableRoleTest : public ::testing::Test
{
public:
    VariableRoleTest()
        : module(nullptr)
    {}
protected:
    void setUp(const char* moduleStr)
    {
        module = llvm::parseAssemblyString(moduleStr, error, llvmContext);
        if (module == nullptr) {
            error.print("VariableRoleTest", llvm::errs());
            FAIL() << "Failed to construct LLVM module!\n";
            return;
        }
    }

protected:
    llvm::LLVMContext llvmContext;
    llvm::SMDiagnostic error;
    std::unique_ptr<llvm::Module> module;
};

TEST_F(VariableRoleTest, TestCompareRoles)
{
    setUp(R"ASM(
declare i32 @read()

define i32 @main() {
bb:
  %tmp = call i32 @read()
  br label %bb8

bb8:                                              ; preds = %bb41, %bb
  %.03 = phi i32 [ 0, %bb ], [ %.36, %bb41 ]
  %.01 = phi i32 [ %tmp, %bb ], [ %.3, %bb41 ]
  %.0 = phi i32 [ 0, %bb ], [ %.1, %bb41 ]
  br label %bb9

bb9:                                              ; preds = %bb8
  %tmp10 = call i32 @read()
  %tmp11 = icmp ne i32 %tmp10, 0
  %tmp12 = zext i1 %tmp11 to i8
  %tmp13 = icmp eq i32 %.01, 1
  br i1 %tmp13, label %bb14, label %bb17

bb14:                                             ; preds = %bb9
  %tmp15 = icmp eq i32 %.03, 1
  br i1 %tmp15, label %bb16, label %bb17

bb16:                                             ; preds = %bb14
  br label %bb40

bb17:                                             ; preds = %bb14, %bb9
  %tmp18 = icmp eq i32 %.01, 2
  br i1 %tmp18, label %bb19, label %bb22

bb19:                                             ; preds = %bb17
  %tmp20 = icmp eq i32 %.03, 1
  br i1 %tmp20, label %bb21, label %bb22

bb21:                                             ; preds = %bb19
  br label %bb38

bb22:                                             ; preds = %bb19, %bb17
  %tmp23 = trunc i8 %tmp12 to i1
  br i1 %tmp23, label %bb27, label %bb24

bb24:                                             ; preds = %bb22
  %tmp25 = icmp eq i32 %.01, 0
  br i1 %tmp25, label %bb26, label %bb27

bb26:                                             ; preds = %bb24
  br label %bb37

bb27:                                             ; preds = %bb24, %bb22
  %tmp28 = icmp eq i32 %.01, 1
  br i1 %tmp28, label %bb29, label %bb30

bb29:                                             ; preds = %bb27
  br label %bb40

bb30:                                             ; preds = %bb27
  %tmp31 = icmp eq i32 %.01, 0
  br i1 %tmp31, label %bb32, label %bb35

bb32:                                             ; preds = %bb30
  %tmp33 = icmp eq i32 %.03, 0
  br i1 %tmp33, label %bb34, label %bb35

bb34:                                             ; preds = %bb32
  br label %bb40

bb35:                                             ; preds = %bb32, %bb30
  br label %bb36

bb36:                                             ; preds = %bb35
  br label %bb37

bb37:                                             ; preds = %bb36, %bb26
  %.14 = phi i32 [ %.03, %bb36 ], [ %.01, %bb26 ]
  %.12 = phi i32 [ %.01, %bb36 ], [ 5, %bb26 ]
  br label %bb38

bb38:                                             ; preds = %bb37, %bb21
  %.25 = phi i32 [ %.01, %bb21 ], [ %.14, %bb37 ]
  %.2 = phi i32 [ 5, %bb21 ], [ %.12, %bb37 ]
  br label %bb39

bb39:                                             ; preds = %bb38
  br label %bb40

bb40:                                             ; preds = %bb39, %bb34, %bb29, %bb16
  %.07 = phi i32 [ 1, %bb16 ], [ 0, %bb39 ], [ 1, %bb29 ], [ 1, %bb34 ]
  %.36 = phi i32 [ %.03, %bb16 ], [ %.25, %bb39 ], [ %.03, %bb29 ], [ %.03, %bb34 ]
  %.3 = phi i32 [ %.01, %bb16 ], [ %.2, %bb39 ], [ %.01, %bb29 ], [ %.01, %bb34 ]
  %.1 = phi i32 [ 1, %bb16 ], [ %.0, %bb39 ], [ 1, %bb29 ], [ 0, %bb34 ]
  switch i32 %.07, label %bb42 [
    i32 0, label %bb41
  ]

bb41:                                             ; preds = %bb40
  br label %bb8

bb42:                                             ; preds = %bb40
  ret i32 %.1
}
)ASM");

    VariableRoleAnalysis::Create(module->getFunction("main"));
}