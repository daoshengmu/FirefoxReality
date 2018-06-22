/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRBrowser.h"
#include "vrb/ConcreteClass.h"
#include "vrb/Logger.h"
#include "JNIUtil.h"

namespace {

static const char* kDispatchCreateWidgetName = "dispatchCreateWidget";
static const char* kDispatchCreateWidgetSignature = "(ILandroid/graphics/SurfaceTexture;II)V";
static const char* kHandleMotionEventName = "handleMotionEvent";
static const char* kHandleMotionEventSignature = "(IIZFF)V";
static const char* kHandleScrollEventName = "handleScrollEvent";
static const char* kHandleScrollEventSignature = "(IIFF)V";
static const char* kHandleAudioPoseName = "handleAudioPose";
static const char* kHandleAudioPoseSignature = "(FFFFFFF)V";
static const char* kHandleGestureName = "handleGesture";
static const char* kHandleGestureSignature = "(I)V";
static const char* kRegisterExternalContextName = "registerExternalContext";
static const char* kRegisterExternalContextSignature = "(J)V";
static const char* kPauseCompositorName = "pauseGeckoViewCompositor";
static const char* kPauseCompositorSignature = "()V";
static const char* kResumeCompositorName = "resumeGeckoViewCompositor";
static const char* kResumeCompositorSignature = "()V";

static JNIEnv* sEnv;
static jobject sActivity;
static jmethodID sDispatchCreateWidget;
static jmethodID sHandleMotionEvent;
static jmethodID sHandleScrollEvent;
static jmethodID sHandleAudioPose;
static jmethodID sHandleGesture;
static jmethodID sRegisterExternalContext;
static jmethodID sPauseCompositor;
static jmethodID sResumeCompositor;

}

namespace crow {

void
VRBrowser::InitializeJava(JNIEnv* aEnv, jobject aActivity) {
  if (aEnv == sEnv) {
    return;
  }
  sEnv = aEnv;
  if (!sEnv) {
    return;
  }
  sActivity = sEnv->NewGlobalRef(aActivity);
  jclass browserClass = sEnv->GetObjectClass(sActivity);
  if (!browserClass) {
    return;
  }

  sDispatchCreateWidget = FindJNIMethodID(sEnv, browserClass, kDispatchCreateWidgetName, kDispatchCreateWidgetSignature);
  sHandleMotionEvent = FindJNIMethodID(sEnv, browserClass, kHandleMotionEventName, kHandleMotionEventSignature);
  sHandleScrollEvent = FindJNIMethodID(sEnv, browserClass, kHandleScrollEventName, kHandleScrollEventSignature);
  sHandleAudioPose = FindJNIMethodID(sEnv, browserClass, kHandleAudioPoseName, kHandleAudioPoseSignature);
  sHandleGesture = FindJNIMethodID(sEnv, browserClass, kHandleGestureName, kHandleGestureSignature);
  sRegisterExternalContext = FindJNIMethodID(sEnv, browserClass, kRegisterExternalContextName, kRegisterExternalContextSignature);
  sPauseCompositor = FindJNIMethodID(sEnv, browserClass, kPauseCompositorName, kPauseCompositorSignature);
  sResumeCompositor = FindJNIMethodID(sEnv, browserClass, kResumeCompositorName, kResumeCompositorSignature);
}

void
VRBrowser::ShutdownJava() {
  if (!sEnv) {
    return;
  }
  if (sActivity) {
    sEnv->DeleteGlobalRef(sActivity);
    sActivity = nullptr;
  }

  sDispatchCreateWidget = nullptr;
  sHandleMotionEvent = nullptr;
  sHandleScrollEvent = nullptr;
  sHandleAudioPose = nullptr;
  sHandleGesture = nullptr;
  sRegisterExternalContext = nullptr;
  sPauseCompositor = nullptr;
  sResumeCompositor = nullptr;
  sEnv = nullptr;
}

void
VRBrowser::DispatchCreateWidget(jint aWidgetHandle, jobject aSurface, jint aWidth, jint aHeight) {
  if (!ValidateMethodID(sEnv, sActivity, sDispatchCreateWidget, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sDispatchCreateWidget, aWidgetHandle, aSurface, aWidth, aHeight);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::HandleMotionEvent(jint aWidgetHandle, jint aController, jboolean aPressed, jfloat aX, jfloat aY) {
  if (!ValidateMethodID(sEnv, sActivity, sHandleMotionEvent, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sHandleMotionEvent, aWidgetHandle, aController, aPressed, aX, aY);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::HandleScrollEvent(jint aWidgetHandle, jint aController, jfloat aX, jfloat aY) {
  if (!ValidateMethodID(sEnv, sActivity, sHandleScrollEvent, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sHandleScrollEvent, aWidgetHandle, aController, aX, aY);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::HandleAudioPose(jfloat qx, jfloat qy, jfloat qz, jfloat qw, jfloat px, jfloat py, jfloat pz) {
  if (!ValidateMethodID(sEnv, sActivity, sHandleAudioPose, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sHandleAudioPose, qx, qy, qz, qw, px, py, pz);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::HandleGesture(jint aType) {
  if (!ValidateMethodID(sEnv, sActivity, sHandleGesture, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sHandleGesture, aType);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::RegisterExternalContext(jlong aContext) {
  if (!ValidateMethodID(sEnv, sActivity, sRegisterExternalContext, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sRegisterExternalContext, aContext);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::PauseCompositor() {
  if (!ValidateMethodID(sEnv, sActivity, sPauseCompositor, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sPauseCompositor);
  CheckJNIException(sEnv, __FUNCTION__);
}

void
VRBrowser::ResumeCompositor() {
  if (!ValidateMethodID(sEnv, sActivity, sResumeCompositor, __FUNCTION__)) { return; }
  sEnv->CallVoidMethod(sActivity, sResumeCompositor);
  CheckJNIException(sEnv, __FUNCTION__);
}

} // namespace crow
