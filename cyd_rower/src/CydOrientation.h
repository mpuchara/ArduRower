#pragma once
#include <stdint.h>
// Landscape rotations for the CYD ST7789 are 1 and 3; both are 320x240.
// The original saved touch affine transform is tied to its rotation.
namespace cydview {
static constexpr uint8_t kDefaultRotation=1;
inline uint8_t checkedRotation(uint8_t rotation) {
  return (rotation==1 || rotation==3) ? rotation : kDefaultRotation;
}
inline uint8_t toggleRotation(uint8_t rotation) {
  return checkedRotation(rotation)==1 ? 3 : 1;
}
inline void mapTouchToDisplay(float &x,float &y,uint8_t calibrationRotation,
                              uint8_t displayRotation) {
  if(checkedRotation(calibrationRotation)==checkedRotation(displayRotation))
    return;
  x=319.0f-x;
  y=239.0f-y;
}
} // namespace cydview
