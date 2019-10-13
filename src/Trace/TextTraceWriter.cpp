#include "gazer/Trace/TraceWriter.h"
#include "gazer/Core/LiteralExpr.h"

#include <llvm/Support/raw_ostream.h>

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
            mOS << INDENT << event.getVariableName() << " := ";

            if (llvm::isa<UndefExpr>(expr.get())) {
                mOS << "???";
                if (mPrintBv) {
                    mOS << "\t";
                    mOS.indent(64);
                }
            } else {
                expr->print(mOS);
                if (expr->getType().isBvType() && mPrintBv) {
                    std::bitset<64> bits;

                    if (expr->getType().isBvType()) {
                        auto apVal = llvm::dyn_cast<BvLiteralExpr>(expr.get())->getValue();
                        bits = apVal.getLimitedValue();
                    } else if (expr->getType().isFloatType()) {
                        auto fltVal = llvm::dyn_cast<FloatLiteralExpr>(expr.get())->getValue();
                        bits = fltVal.bitcastToAPInt().getLimitedValue();
                    } else {
                        llvm_unreachable("Unknown bit-vector type!");
                    }

                    mOS << "\t(0b" << bits.to_string() << ")";
                }
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
                << " in function " << event.getFunctionName() << ":\n";
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
