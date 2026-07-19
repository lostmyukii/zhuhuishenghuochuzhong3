#pragma once

#include "smartlife_models.h"

namespace smartlife {

ControlState defaultControlState();
ControlOutputs bootSafeOutputs();
ControlEvaluation evaluateControl(const ControlInputs& inputs,
                                  const ControlState& currentState);

CommandDecision decideModeCommand(bool safetyActive);
CommandDecision decideManualActuatorCommand(bool safetyActive);

bool hasAlert(uint8_t activeAlerts, AlertCode code);

}  // namespace smartlife
