#pragma once

class RemoteStreamConfig {
public:
    bool remoteResolution;
    int remoteResolutionWidth;
    int remoteResolutionHeight;
    bool remoteFps;
    int remoteFpsRate;
    int originalStreamWidth;
    int originalStreamHeight;
    float maxBrightness;
    float minBrightness;
    float maxAverageBrightness;
    float sdrWhiteBrightness;

    RemoteStreamConfig(
        bool remoteResolution, int remoteResolutionWidth, int remoteResolutionHeight,
        bool remoteFps, int remoteFpsRate,
        int originalStreamWidth, int originalStreamHeight,
        float maxBrightness = 0, float minBrightness = 0, float maxAverageBrightness = 0,
        float sdrWhiteBrightness = 0)
    {
        this->remoteResolution = remoteResolution;
        this->remoteResolutionWidth = remoteResolutionWidth;
        this->remoteResolutionHeight = remoteResolutionHeight;
        this->remoteFps = remoteFps;
        this->remoteFpsRate = remoteFpsRate;
        this->originalStreamWidth = originalStreamWidth;
        this->originalStreamHeight = originalStreamHeight;
        this->maxBrightness = maxBrightness;
        this->minBrightness = minBrightness;
        this->maxAverageBrightness = maxAverageBrightness;
        this->sdrWhiteBrightness = sdrWhiteBrightness;
    }
};
