#include "fps.h"

bool FramesPerSecondCounter::tick(float deltaSeconds, bool frameRendered)
{
	if (frameRendered) {
		numFrames++;
	}
	accumulatedTime += deltaSeconds;
	if (accumulatedTime < avgInternalSec) {
		return false;
	}
	currentFPS = static_cast<float>(numFrames / accumulatedTime);
	numFrames = 0;
	accumulatedTime = 0;
	return true;
}
