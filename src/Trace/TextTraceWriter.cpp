#include "gazer/Trace/TraceWriter.h"
#include "gazer/Core/LiteralExpr.h"
#include "gazer/ADT/StringUtils.h"

#include <llvm/Support/raw_ostream.h>
#include <llvm/ADT/SmallString.h>

#include <bitset>

using namespace gazer;

namespace
{
    class TextTraceWriter : public TraceWriter
    {
    public:
        explicit TextTraceWriter(llvm::raw_ostream& os, bool printBv = true)
            : TraceWriter(os), mPrintBv(printBv), mFuncEntries(0)
        {}

        static constexpr auto INDENT = "  ";

        void visit(AssignTraceEvent& event) override
        {
            ExprRef<AtomicExpr> expr = event.getExpr();
            mOS << INDENT << event.getVariable().getName() << " := ";

            if (llvm::isa<UndefExpr>(expr.get())) {
                mOS << "???";
            } else if (auto bv = llvm::dyn_cast<BvLiteralExpr>(expr)) {
                TraceVariable var = event.getVariable();
                unsigned varSize = var.getSize();

                switch (var.getRepresentation()) {
                    case TraceVariable::Rep_Unknown:
                        bv->print(mOS);
                        break;
                    case TraceVariable::Rep_Bool:
                        if (bv->isZero()) {
                            mOS << "false";
                        } else {
                            mOS << "true";
                        }
                        break;
                    case TraceVariable::Rep_Signed:
                        bv->getValue().zextOrSelf(var.getSize()).print(mOS, /*isSigned=*/true);
                        break;
                    case TraceVariable::Rep_Char: // TODO
                    case TraceVariable::Rep_Unsigned:
                        bv->getValue().zextOrSelf(var.getSize()).print(mOS, /*isSigned=*/false);
                        break;
                    case TraceVariable::Rep_Float:
                        llvm_unreachable("Cannot represent a int BV type as a float!");
                }

                if (mPrintBv) {
                    llvm::SmallString<64> bits;
                    bv->getValue().zextOrSelf(32).toString(bits, 2, false, false);
                    mOS << "\t(0b";
                    if (bits.size() < varSize) {
                        for (int i = 0; i < varSize - bits.size(); ++i) {
                            mOS.write('0');
                        }
                    }
                    mOS << bits << ')';
                }
            } else {
                expr->print(mOS);
            }

            auto location = event.getLocation();
            if (location.getLine() != 0) {
                mOS << "\t at "
                    << location.getLine()
                    << ":"
                    << location.getColumn()
                    << "";
            }
            mOS << "\n";
        }

        void visit(FunctionEntryEvent& event) override
        {
            mOS << "#" << (mFuncEntries++)
                << " in function " << event.getFunctionName();
            mOS << '(';
            join_print_as(mOS, event.arg_begin(), event.arg_end(), ",", [](auto& os, auto& ptr) {
                ptr->print(os);
            });
            mOS << ')';
            mOS << ":\n";
        }

        void visit(FunctionReturnEvent& event) override
        {
            mOS << INDENT << "return ";
            if (event.hasReturnValue()) {
                auto ret = event.getReturnValue();
                if (event.getReturnValue()->getKind() == Expr::Undef) {
                    mOS << "???";
                } else {
                    event.getReturnValue()->print(mOS);
                }
            } else {
                mOS << "void";
            }
            mOS << "\n";
        }

        void visit(FunctionCallEvent& event) override
        {
            mOS << INDENT << "call "
                << event.getFunctionName() << "() returned ";
            if (event.getReturnValue()->getKind() == Expr::Undef) {
                mOS << "???";
            } else {
                event.getReturnValue()->print(mOS);
            }
            mOS << "\t";

            auto location = event.getLocation();
            if (location.getLine() != 0) {
                mOS << "\t at "
                    << location.getLine()
                    << ":"
                    << location.getColumn()
                    << "";
            }
            mOS << "\n";
        }

        void visit(UndefinedBehaviorEvent& event) override
        {
            mOS << INDENT << "Undefined behavior (read of an undefined value: ";
            if (event.getPickedValue()->getKind() == Expr::Undef) {
                mOS << "???";
            } else {
                event.getPickedValue()->print(mOS);
            }

            mOS << ")\t";

            auto location = event.getLocation();
            if (location.getLine() != 0) {
                mOS << "\t at "
                    << location.getLine()
                    << ":"
                    << location.getColumn()
                    << "";
            }
            mOS << "\n";
        }

    private:
        bool mPrintBv;
        size_t mFuncEntries;
    };
} // end anonymous namespace

namespace gazer::trace
{
    std::unique_ptr<TraceWriter> CreateTextWriter(llvm::raw_ostream& os, bool printBv)
    {
        return std::make_unique<TextTraceWriter>(os, printBv);
    }
} // end namespace gazer::trace
