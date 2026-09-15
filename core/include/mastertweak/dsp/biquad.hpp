#pragma once

#include "audioplugins/common/dsp/Biquad.h"

namespace mt::dsp {

using BiquadCoeffs = audioplugins::common::dsp::BiquadCoeffs;
using BiquadState  = audioplugins::common::dsp::BiquadState;
using audioplugins::common::dsp::biquadProcess;

} // namespace mt::dsp
