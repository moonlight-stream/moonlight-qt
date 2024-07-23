/*
Copyright © 2024 Apple Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
documentation files (the "Software"), to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#pragma once

#include <AudioToolbox/AudioToolbox.h>

class AllocatedAudioBufferList
{
public:
    AllocatedAudioBufferList(UInt32 channelCount, uint16_t bufferSize)
    {

        mBufferList = static_cast<AudioBufferList *>(malloc(sizeof(AudioBufferList) + (sizeof(AudioBuffer) * channelCount)));
        mBufferList->mNumberBuffers = channelCount;
        for (UInt32 c = 0;  c < channelCount; ++c) {
            mBufferList->mBuffers[c].mNumberChannels = 1;
            mBufferList->mBuffers[c].mDataByteSize = bufferSize * sizeof(float);
            mBufferList->mBuffers[c].mData = malloc(sizeof(float) * bufferSize);
        }
    }

    AllocatedAudioBufferList(const AllocatedAudioBufferList&) = delete;

    AllocatedAudioBufferList& operator=(const AllocatedAudioBufferList&) = delete;

    ~AllocatedAudioBufferList()
    {
        if (mBufferList == nullptr) { return; }

        for (UInt32 i = 0; i < mBufferList->mNumberBuffers; ++i) {
            free(mBufferList->mBuffers[i].mData);
        }
        free(mBufferList);
        mBufferList = nullptr;
    }

    AudioBufferList * _Nonnull get()
    {
        return mBufferList;
    }

private:
    AudioBufferList * _Nonnull mBufferList  = { nullptr };
};
