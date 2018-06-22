/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalVR.h"

#include "vrb/ConcreteClass.h"
#include "vrb/Matrix.h"
#include "vrb/Quaternion.h"
#include "vrb/Vector.h"
#include "moz_external_vr.h"
#include <pthread.h>

namespace {

const float SecondsToNanoseconds = 1e9f;
const int SecondsToNanosecondsI32 = int(1e9);
const int MicrosecondsToNanoseconds = 1000;

class Lock {
  pthread_mutex_t& mMutex;
  bool mLocked;
public:
  Lock(pthread_mutex_t& aMutex) : mMutex(aMutex), mLocked(false) {
    if (pthread_mutex_lock(&mMutex) == 0) {
      mLocked = true;
    }
  }

  ~Lock() {
    if (mLocked) {
      pthread_mutex_unlock(&mMutex);
    }
  }

  bool IsLocked() {
    return mLocked;
  }

private:
  Lock() = delete;
  VRB_NO_DEFAULTS(Lock)
  VRB_NO_NEW_DELETE
};

class Wait {
  pthread_mutex_t& mMutex;
  pthread_cond_t& mCond;
  const float mWait;
  bool mLocked;
public:
  Wait(pthread_mutex_t& aMutex, pthread_cond_t& aCond, float aWait = 0.0f)
      : mMutex(aMutex)
      , mCond(aCond)
      , mWait(aWait)
      , mLocked(false)
  {}

  ~Wait() {
    if (mLocked) {
      pthread_mutex_unlock(&mMutex);
    }
  }

  void DoWait() {
    if (mLocked || pthread_mutex_lock(&mMutex) == 0) {
      mLocked = true;
      if (mWait == 0.0f) {
        pthread_cond_wait(&mCond, &mMutex);
      } else {
        float sec = 0;
        float nsec = modff(mWait, &sec);
        struct timeval tv;
        struct timespec ts;
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + int(sec);
        ts.tv_nsec = (tv.tv_usec * MicrosecondsToNanoseconds) + int(SecondsToNanoseconds * nsec);
        if (ts.tv_nsec >= SecondsToNanosecondsI32) {
          ts.tv_nsec -= SecondsToNanosecondsI32;
          ts.tv_sec++;
        }
        pthread_cond_timedwait(&mCond, &mMutex, &ts);
      }
    }
  }

  bool IsLocked() {
    return mLocked;
  }

  void Lock() {
    if (mLocked) {
      return;
    }

    if (pthread_mutex_lock(&mMutex) == 0) {
      mLocked = true;
    }
  }
  void Unlock() {
    if (mLocked) {
      mLocked = false;
      pthread_mutex_unlock(&mMutex);
    }
  }

private:
  Wait() = delete;
  VRB_NO_DEFAULTS(Wait)
  VRB_NO_NEW_DELETE
};

void
SignalCond(pthread_mutex_t& aMutex, pthread_cond_t& aCond) {
  if (pthread_mutex_lock(&aMutex) == 0) {
    pthread_cond_signal(&aCond);
    pthread_mutex_unlock(&aMutex);
  }
}

} // namespace

namespace crow {

struct ExternalVR::State {
  mozilla::gfx::VRExternalShmem data;
  mozilla::gfx::VRSystemState system;
  mozilla::gfx::VRBrowserState browser;
  device::CapabilityFlags deviceCapabilities;
  vrb::Vector eyeOffsets[device::EyeCount];

  State() : deviceCapabilities(0) {
    memset(&data, 0, sizeof(mozilla::gfx::VRExternalShmem));
    memset(&system, 0, sizeof(mozilla::gfx::VRSystemState));
    memset(&browser, 0, sizeof(mozilla::gfx::VRBrowserState));
    data.version = mozilla::gfx::kVRExternalVersion;
    data.size = sizeof(mozilla::gfx::VRExternalShmem);
    pthread_mutex_init(&(data.systemMutex), nullptr);
    pthread_mutex_init(&(data.browserMutex), nullptr);
    pthread_cond_init(&(data.systemCond), nullptr);
    pthread_cond_init(&(data.browserCond), nullptr);

    system.displayState.mIsConnected = true;
    system.displayState.mIsMounted = true;
    system.enumerationCompleted = true;
    const vrb::Matrix identity = vrb::Matrix::Identity();
    memcpy(&(system.sensorState.leftViewMatrix), identity.Data(), sizeof(system.sensorState.leftViewMatrix));
    memcpy(&(system.sensorState.rightViewMatrix), identity.Data(), sizeof(system.sensorState.rightViewMatrix));
  }

  ~State() {
    pthread_mutex_destroy(&(data.systemMutex));
    pthread_mutex_destroy(&(data.browserMutex));
    pthread_cond_destroy(&(data.systemCond));
    pthread_cond_destroy(&(data.browserCond));
  }
};

ExternalVRPtr
ExternalVR::Create() {
  return std::make_shared<vrb::ConcreteClass<ExternalVR, ExternalVR::State> >();
}

mozilla::gfx::VRExternalShmem*
ExternalVR::GetSharedData() {
  return &(m.data);
}

void
ExternalVR::SetDeviceName(const std::string& aName) {
  if (aName.length() == 0) {
    return;
  }
  strncpy(m.system.displayState.mDisplayName, aName.c_str(),
          mozilla::gfx::kVRDisplayNameMaxLen - 1);
  m.system.displayState.mDisplayName[mozilla::gfx::kVRDisplayNameMaxLen - 1] = '\0';
}

void
ExternalVR::SetCapabilityFlags(const device::CapabilityFlags aFlags) {
  uint16_t result = 0;
  if (device::Position & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_Position);
  }
  if (device::Orientation & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_Orientation);
  }
  if (device::Present & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_Present);
  }
  if (device::AngularAcceleration & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_AngularAcceleration);
  }
  if (device::LinearAcceleration & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_LinearAcceleration);
  }
  if (device::StageParameters & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_StageParameters);
  }
  if (device::MountDetection & aFlags) {
    result |= static_cast<uint16_t>(mozilla::gfx::VRDisplayCapabilityFlags::Cap_MountDetection);
  }
  m.deviceCapabilities = aFlags;
  m.system.displayState.mCapabilityFlags = static_cast<mozilla::gfx::VRDisplayCapabilityFlags>(result);
  m.system.sensorState.flags = m.system.displayState.mCapabilityFlags;
}

void
ExternalVR::SetFieldOfView(const device::Eye aEye, const double aLeftDegrees,
                           const double aRightDegrees,
                           const double aTopDegrees,
                           const double aBottomDegrees) {
  mozilla::gfx::VRDisplayState::Eye which = (aEye == device::Eye::Right
                                             ? mozilla::gfx::VRDisplayState::Eye_Right
                                             : mozilla::gfx::VRDisplayState::Eye_Left);
  m.system.displayState.mEyeFOV[which].upDegrees = aTopDegrees;
  m.system.displayState.mEyeFOV[which].rightDegrees = aRightDegrees;
  m.system.displayState.mEyeFOV[which].downDegrees = aBottomDegrees;
  m.system.displayState.mEyeFOV[which].leftDegrees = aLeftDegrees;
}

void
ExternalVR::SetEyeOffset(const device::Eye aEye, const float aX, const float aY, const float aZ) {
  mozilla::gfx::VRDisplayState::Eye which = (aEye == device::Eye::Right
                                             ? mozilla::gfx::VRDisplayState::Eye_Right
                                             : mozilla::gfx::VRDisplayState::Eye_Left);
  m.system.displayState.mEyeTranslation[which].x = aX;
  m.system.displayState.mEyeTranslation[which].y = aY;
  m.system.displayState.mEyeTranslation[which].z = aZ;
  m.eyeOffsets[device::EyeIndex(aEye)].Set(aX, aY, aZ);
}

void
ExternalVR::SetEyeResolution(const int32_t aWidth, const int32_t aHeight) {
  m.system.displayState.mEyeResolution.width = aWidth;
  m.system.displayState.mEyeResolution.height = aHeight;
}

void
ExternalVR::PushSystemState() {
  Lock lock(m.data.systemMutex);
  if (lock.IsLocked()) {
    memcpy(&(m.data.state), &(m.system), sizeof(mozilla::gfx::VRSystemState));
  }
}

void
ExternalVR::PullBrowserState() {
  Lock lock(m.data.browserMutex);
  if (lock.IsLocked()) {
    memcpy(&(m.browser), &(m.data.browserState), sizeof(mozilla::gfx::VRBrowserState));
  }
}

bool
ExternalVR::IsFirstPresentingFrame() const {
  return IsPresenting() && m.browser.layerState[0].layer_stereo_immersive.mFrameId == 0;
}

bool
ExternalVR::IsPresenting() const {
  VRB_LOG("IsPresenting=%s",((m.browser.layerState[0].type == mozilla::gfx::VRLayerType::LayerType_Stereo_Immersive)?"TRUE":"FALSE"));
  return (m.browser.layerState[0].type == mozilla::gfx::VRLayerType::LayerType_Stereo_Immersive);
}

void
ExternalVR::RequestFrame(const vrb::Matrix& aHeadTransform) {
  memcpy(&(m.system.sensorState.orientation), vrb::Quaternion(aHeadTransform).Data(),
         sizeof(m.system.sensorState.orientation));
  memcpy(&(m.system.sensorState.position), aHeadTransform.GetTranslation().Data(),
         sizeof(m.system.sensorState.position));
  /*
  vrb::Matrix leftView = vrb::Matrix::Position(m.eyeOffsets[device::EyeIndex(device::Eye::Left)]).PreMultiply(aHeadTransform);
  vrb::Matrix rightView = vrb::Matrix::Position(m.eyeOffsets[device::EyeIndex(device::Eye::Right)]).PreMultiply(aHeadTransform);
  memcpy(&(m.system.sensorState.leftViewMatrix), leftView.Data(),
         sizeof(m.system.sensorState.leftViewMatrix));
  memcpy(&(m.system.sensorState.rightViewMatrix), rightView.Data(),
         sizeof(m.system.sensorState.rightViewMatrix));
         */
  PushSystemState();
  SignalCond(m.data.browserMutex, m.data.browserCond);
  Wait wait(m.data.browserMutex, m.data.browserCond);
  bool done = false;
  while (!done) {
    VRB_LOG("ABOUT TO WAIT FOR FRAME");
    wait.DoWait();
    memcpy(&(m.browser), &(m.data.browserState), sizeof(mozilla::gfx::VRBrowserState));
    VRB_LOG("m.browser.layerState[0].layer_stereo_immersive.mInputFrameId=%llu m.system.sensorState.inputFrameID=%llu", m.browser.layerState[0].layer_stereo_immersive.mInputFrameId, m.system.sensorState.inputFrameID);
    VRB_LOG("m.browser.layerState[0].layer_stereo_immersive.mFrameId=%llu", m.browser.layerState[0].layer_stereo_immersive.mFrameId);
    if (m.browser.layerState[0].layer_stereo_immersive.mInputFrameId == m.system.sensorState.inputFrameID) {
      done = true;
      m.system.displayState.mLastSubmittedFrameId = m.browser.layerState[0].layer_stereo_immersive.mFrameId;
      m.system.displayState.mLastSubmittedFrameSuccessful = true;
    }
  }
  m.system.sensorState.inputFrameID++;
  wait.Unlock();
  //PushSystemState();
}

void
ExternalVR::GetFrameResult(int32_t& aSurfaceHandle, device::EyeRect& aLeftEye, device::EyeRect& aRightEye) const {
  aSurfaceHandle = (int32_t)m.browser.layerState[0].layer_stereo_immersive.mTextureHandle;
  mozilla::gfx::VRLayerEyeRect& left = m.browser.layerState[0].layer_stereo_immersive.mLeftEyeRect;
  mozilla::gfx::VRLayerEyeRect& right = m.browser.layerState[0].layer_stereo_immersive.mRightEyeRect;
  aLeftEye = device::EyeRect(left.x, left.y, left.width, left.height);
  aRightEye = device::EyeRect(right.x, right.y, right.width, right.height);
}

void
ExternalVR::StopPresenting() {
  m.system.displayState.mPresentingGeneration++;
}

ExternalVR::ExternalVR(State& aState) : m(aState) {
  PushSystemState();
}

ExternalVR::~ExternalVR() {}

}
