#pragma once

#include "pacer.h"

class DisplayLinkVsyncSourceFactory
{
public:
    static
    IVsyncSource* createVsyncSource(Pacer* pacer);
};

