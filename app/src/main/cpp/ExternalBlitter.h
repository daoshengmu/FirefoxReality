/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VRBROWSER_EXTERNALBLITTER_H
#define VRBROWSER_EXTERNALBLITTER_H

#include "vrb/MacroUtils.h"
#include "vrb/Forward.h"
#include "vrb/ResourceGL.h"
#include "Device.h"
#include "ExternalVR.h"
#include <memory>

namespace crow {

class ExternalBlitter;
typedef std::shared_ptr<ExternalBlitter> ExternalBlitterPtr;

class ExternalBlitter : protected vrb::ResourceGL {
public:
  static ExternalBlitterPtr Create(vrb::ContextWeak& aContext);
  void Update(const int32_t aSurfaceHandle, const device::EyeRect& aLeftEye, const device::EyeRect& aRightEye);
  void Draw(const device::Eye aEye);
  void Finish();
protected:
  struct State;
  ExternalBlitter(State& aState, vrb::ContextWeak& aContext);
  ~ExternalBlitter();
  void InitializeGL(vrb::Context& aContext) override;
  void ShutdownGL(vrb::Context& aContext) override;
private:
  State& m;
  ExternalBlitter() = delete;
  VRB_NO_DEFAULTS(ExternalBlitter)
};

} // namespace crow

#endif //VRBROWSER_EXTERNALBLITTER_H
