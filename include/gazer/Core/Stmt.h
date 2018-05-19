#ifndef _GAZER_CORE_STMT_H
#define _GAZER_CORE_STMT_H

namespace gazer
{

class Stmt
{
public:
    enum StmtKind
    {
        Stmt_Skip,
        Stmt_Assume,
        Stmt_Assign,
        Stmt_Assert
    };

protected:
    Stmt(StmtKind kind)
        : mKind(kind)
    {}

public:
    StmtKind getKind() const { return mKind; }

private:
    StmtKind mKind;
};


}

#endif
