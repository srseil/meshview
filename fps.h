#pragma once

#include <cassert>

class FramesPerSecondCounter {
public:
	explicit FramesPerSecondCounter(float avgInternalSec = 0.5f) : avgInternalSec(avgInternalSec) { assert(avgInternalSec > 0.0f); }
	bool tick(float deltaSeconds, bool frameRendered = true);
	float getFPS() const { return currentFPS; }
private:
	const float avgInternalSec = 0.5f;
	unsigned int numFrames = 0;
	double accumulatedTime = 0;
	float currentFPS = 0.0f;
};
