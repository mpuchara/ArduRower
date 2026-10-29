#pragma once
// XY touchscreen gesture recognition independent of Arduino / TFT libraries.
// Handles XPT2046 axis swap/reversal and previously saved touch transforms.
#include <math.h>
namespace cydtouch {
inline bool pageSwipe(int rawDx,int rawDy,
                      bool calibrationValid,float mappedDx,float mappedDy) {
  const float mx=fabsf(mappedDx),my=fabsf(mappedDy);
  const int ax=rawDx<0?-rawDx:rawDx;
  const int ay=rawDy<0?-rawDy:rawDy;
  const int major=ax>ay?ax:ay;
  const int minor=ax<ay?ax:ay;
  const bool mapped=calibrationValid && mx>=42.0f && mx>my*1.35f;
  const bool fallback=major>=245 && major>minor*1.65f;
  return mapped || fallback; // either left or right, raw X or raw Y
}
}
