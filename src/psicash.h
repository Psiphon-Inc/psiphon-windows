#pragma once

#include "3rdParty/psicash/psicash.hpp"
#include "tstring.h"


namespace psicash {

class Lib : public PsiCash {
public:
    static Lib& _()
    {
        // Instantiated on first use.
        // Guaranteed to be destroyed.
        static Lib instance;
        return instance;
    }

private:
    Lib();
public:
    Lib(const Lib&) = delete;
    Lib& operator=(Lib const&) = delete;

    virtual ~Lib();
    error::Error Init();

private:
    HANDLE m_mutex;
    bool m_initialized;
};

} // namespace psicash
