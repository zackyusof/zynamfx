#pragma once

#include "UiClock.h"
#include "backend/Backend.h"

#include <cmath>

namespace t3k::ui {

class TempoStore : private UiClock::Listener {
public:
  struct State {
    float bpm = 0.0f;
    int lock = 0;  // 0 no signal, 1 clock, 2 locked
    bool running = false;
    bool stale = false;
    bool activity = false;
  };

  struct Listener {
    virtual ~Listener() = default;
    virtual void tempoChanged() = 0;
  };

  TempoStore(Backend& backend, UiClock& clock) : backend_(backend), clock_(clock) {}
  ~TempoStore() override { clock_.removeListener(this); }

  void addListener(Listener* listener) {
    listeners_.add(listener);
    clock_.addListener(this);
  }
  void removeListener(Listener* listener) {
    listeners_.remove(listener);
    if (listeners_.isEmpty()) clock_.removeListener(this);
  }

  const State& state() const { return state_; }

private:
  void tick() override {
    const auto value = backend_.getMidiClockState();
    if (!value.isObject()) return;
    State next;
    next.bpm = static_cast<float>(value.getProperty("bpm", 0.0));
    next.lock = static_cast<int>(value.getProperty("state", 0));
    next.running = static_cast<int>(value.getProperty("transport", 0)) != 0;
    next.stale = static_cast<bool>(value.getProperty("stale", false));
    next.activity = static_cast<bool>(value.getProperty("activity", false));
    if (next.lock == state_.lock && next.running == state_.running && next.stale == state_.stale &&
        next.activity == state_.activity && std::abs(next.bpm - state_.bpm) < 0.05f)
      return;
    state_ = next;
    listeners_.call([](Listener& listener) { listener.tempoChanged(); });
  }

  Backend& backend_;
  UiClock& clock_;
  State state_;
  juce::ListenerList<Listener> listeners_;
};

}  // namespace t3k::ui
