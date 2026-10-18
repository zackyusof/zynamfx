#include "WindowCapture.h"

#if !defined(__APPLE__)

namespace t3k::ui {

juce::Image captureComponentWindow(juce::Component& component) {
  return component.createComponentSnapshot(component.getLocalBounds());
}

}  // namespace t3k::ui

#endif
