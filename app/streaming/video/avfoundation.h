#pragma once

#include "decoder.h"

// A factory is required to avoid pulling in
// incompatible Objective-C headers.
class AVFoundationDecoderFactory {
public:
    static
    IVideoDecoder* createDecoder();
};
