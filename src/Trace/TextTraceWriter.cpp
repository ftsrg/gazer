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
        TextTraceWriter(llvm::raw_ostream& os, bool printBv = true)
            : TraceWriter(os), mPrintBv(printBv), mFuncEntries(0)
        {}

        static constexpr char INDENT[] = "  ";

        void visit(AssignTraceEvent& event) override
        {
            std::shared_ptr<AtomicExpr> expr = event.getExpr();
            mOS << INDENT << event.getVariableName() << " := ";
            expr->print(mOS);

            if (mPrintBv) {
                std::bitset<64> bits;

                if (expr->getType().isBvType()) {
                    auto apVal = llvm::dyn_cast<BvLiteralExpr>(expr.get())->getValue();
                    bits = apVal.getLimitedValue();
                } else if (expr->getType().isFloatType()) {
                    auto fltVal = llvm::dyn_cast<FloatLiteralExpr>(expr.get())->getValue();
                    bits = fltVal.bitcastToAPInt().getLimitedValue();
                }

                mOS << "\t(0b" << bits.to_string() << ")";
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

    private:
        bool mPrintBv;
        size_t mFuncEntries;
    };
} // end anonymous namespace

namespace gazer
{
    namespace trace
    {
        std::unique_ptr<TraceWriter> CreateTextWriter(llvm::raw_ostream& os, bool printBv)
        {
            return std::unique_ptr<TextTraceWriter>(new TextTraceWriter(os, printBv));
        }
    } // end namespace trace
} // end namespace gazer
