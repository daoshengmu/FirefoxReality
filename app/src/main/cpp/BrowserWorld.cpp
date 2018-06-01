/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BrowserWorld.h"
#include "ControllerDelegate.h"
#include "Device.h"
#include "DeviceDelegate.h"
#include "ExternalBlitter.h"
#include "ExternalVR.h"
#include "GeckoSurfaceTexture.h"
#include "Widget.h"
#include "WidgetPlacement.h"
#include "VRBrowser.h"
#include "vrb/CameraSimple.h"
#include "vrb/Color.h"
#include "vrb/ConcreteClass.h"
#include "vrb/Context.h"
#include "vrb/CullVisitor.h"
#include "vrb/DrawableList.h"
#include "vrb/Geometry.h"
#include "vrb/GLError.h"
#include "vrb/Group.h"
#include "vrb/Light.h"
#include "vrb/Logger.h"
#include "vrb/Matrix.h"
#include "vrb/NodeFactoryObj.h"
#include "vrb/ParserObj.h"
#include "vrb/RenderState.h"
#include "vrb/SurfaceTextureFactory.h"
#include "vrb/TextureCache.h"
#include "vrb/TextureSurface.h"
#include "vrb/TextureCubeMap.h"
#include "vrb/Toggle.h"
#include "vrb/Transform.h"
#include "vrb/VertexArray.h"
#include "vrb/Vector.h"

#include <array>
#include <functional>
#include <vrb/include/vrb/TextureGL.h>

using namespace vrb;

namespace {

static const int GestureSwipeLeft = 0;
static const int GestureSwipeRight = 1;

static const float kScrollFactor = 20.0f; // Just picked what fell right.
static const float kWorldDPIRatio = 2.0f/720.0f;

static crow::BrowserWorld* sWorld;

class SurfaceObserver;
typedef std::shared_ptr<SurfaceObserver> SurfaceObserverPtr;

class SurfaceObserver : public SurfaceTextureObserver {
public:
  SurfaceObserver(crow::BrowserWorldWeakPtr& aWorld);
  ~SurfaceObserver();
  void SurfaceTextureCreated(const std::string& aName, GLuint aHandle, jobject aSurfaceTexture) override;
  void SurfaceTextureHandleUpdated(const std::string aName, GLuint aHandle) override;
  void SurfaceTextureDestroyed(const std::string& aName) override;
  void SurfaceTextureCreationError(const std::string& aName, const std::string& aReason) override;

protected:
  crow::BrowserWorldWeakPtr mWorld;
};

SurfaceObserver::SurfaceObserver(crow::BrowserWorldWeakPtr& aWorld) : mWorld(aWorld) {}
SurfaceObserver::~SurfaceObserver() {}

void
SurfaceObserver::SurfaceTextureCreated(const std::string& aName, GLuint, jobject aSurfaceTexture) {
  crow::BrowserWorldPtr world = mWorld.lock();
  if (world) {
    world->SetSurfaceTexture(aName, aSurfaceTexture);
  }
}

void
SurfaceObserver::SurfaceTextureHandleUpdated(const std::string, GLuint) {}

void
SurfaceObserver::SurfaceTextureDestroyed(const std::string& aName) {
  crow::BrowserWorldPtr world = mWorld.lock();
  if (world) {
    jobject nullObject = nullptr;
    world->SetSurfaceTexture(aName, nullObject);
  }
}

void
SurfaceObserver::SurfaceTextureCreationError(const std::string& aName, const std::string& aReason) {
  
}

struct Controller {
  int32_t index;
  bool enabled;
  uint32_t widget;
  float pointerX;
  float pointerY;
  int32_t buttonState;
  int32_t lastButtonState;
  bool touched;
  bool wasTouched;
  float touchX;
  float touchY;
  float lastTouchX;
  float lastTouchY;
  float scrollDeltaX;
  float scrollDeltaY;
  TransformPtr transform;
  Matrix transformMatrix;
  
  Controller() : index(-1), enabled(false), widget(0),
                 pointerX(0.0f), pointerY(0.0f),
                 buttonState(0), lastButtonState(0),
                 touched(false), wasTouched(false),
                 touchX(0.0f), touchY(0.0f),
                 lastTouchX(0.0f), lastTouchY(0.0f),
                 scrollDeltaX(0.0f), scrollDeltaY(0.0f),
                 transformMatrix(Matrix::Identity()) {}

  Controller(const Controller& aController) {
    *this = aController;
  }

  ~Controller() {
    Reset();
  }

  Controller& operator=(const Controller& aController) {
    index = aController.index;
    enabled = aController.enabled;
    widget = aController.widget;
    pointerX = aController.pointerX;
    pointerY = aController.pointerY;
    buttonState = aController.buttonState;
    lastButtonState = aController.lastButtonState;
    touched = aController.touched;
    wasTouched = aController.wasTouched;
    touchX = aController.touchX;
    touchY= aController.touchY;
    lastTouchX = aController.lastTouchX;
    lastTouchY = aController.lastTouchY;
    scrollDeltaX = aController.scrollDeltaX;
    scrollDeltaY = aController.scrollDeltaY;
    transform = aController.transform;
    transformMatrix = aController.transformMatrix;
    return *this;
  }

  void Reset() {
    index = -1;
    enabled = false;
    widget = 0;
    pointerX = pointerY = 0.0f;
    buttonState = lastButtonState = 0;
    touched = wasTouched = false;
    touchX = touchY = 0.0f;
    lastTouchX = lastTouchY = 0.0f;
    scrollDeltaX = scrollDeltaY = 0.0f;
    if (transform) {
      transform = nullptr;
    }
    transformMatrix = Matrix::Identity();
  }
};

class ControllerContainer;
typedef std::shared_ptr<ControllerContainer> ControllerContainerPtr;

class ControllerContainer : public crow::ControllerDelegate {
public:
  static ControllerContainerPtr Create();
  ControllerContainer() : modelsLoaded(false) {}
  ~ControllerContainer();
  void SetUpModelsGroup(const int32_t aModelIndex);
  // crow::ControllerDelegate interface
  void CreateController(const int32_t aControllerIndex, const int32_t aModelIndex) override;
  void DestroyController(const int32_t aControllerIndex) override;
  void SetEnabled(const int32_t aControllerIndex, const bool aEnabled) override;
  void SetVisible(const int32_t aControllerIndex, const bool aVisible) override;
  void SetTransform(const int32_t aControllerIndex, const vrb::Matrix& aTransform) override;
  void SetButtonState(const int32_t aControllerIndex, const int32_t aWhichButton, const bool aPressed) override;
  void SetTouchPosition(const int32_t aControllerIndex, const float aTouchX, const float aTouchY) override;
  void EndTouch(const int32_t aControllerIndex) override;
  void SetScrolledDelta(const int32_t aControllerIndex, const float aScrollDeltaX, const float aScrollDeltaY) override;
  std::vector<Controller> list;
  ContextWeak context;
  TogglePtr root;
  bool modelsLoaded;
  std::vector<GroupPtr> models;
  GeometryPtr pointerModel;
  bool Contains(const int32_t aControllerIndex) {
    return (aControllerIndex >= 0) && (aControllerIndex < list.size());
  }
};

ControllerContainerPtr
ControllerContainer::Create() {
  return std::make_shared<ControllerContainer>();
}

ControllerContainer::~ControllerContainer() {
  if (root) {
    root->RemoveFromParents();
    root = nullptr;
  }
}

void
ControllerContainer::SetUpModelsGroup(const int32_t aModelIndex) {
  if (models.size() >= aModelIndex) {
    models.resize((size_t)(aModelIndex + 1));
  }
  if (!models[aModelIndex]) {
    models[aModelIndex] = std::move(Group::Create(context));
  }
}

void
ControllerContainer::CreateController(const int32_t aControllerIndex, const int32_t aModelIndex) {
  if ((size_t)aControllerIndex >= list.size()) {
    list.resize((size_t)aControllerIndex + 1);
  }
  Controller& controller = list[aControllerIndex];
  controller.index = aControllerIndex;
  if (!controller.transform && (aModelIndex >= 0)) {
    SetUpModelsGroup(aModelIndex);
    controller.transform = Transform::Create(context);
    if ((models.size() >= aModelIndex) && models[aModelIndex]) {
      controller.transform->AddNode(models[aModelIndex]);
      if (pointerModel) {
        controller.transform->AddNode(pointerModel);
      }
      if (root) {
        root->AddNode(controller.transform);
        root->ToggleChild(*controller.transform, false);
      }
    } else {
      VRB_LOG("FAILED TO ADD MODEL");
    }
  }
}

void
ControllerContainer::DestroyController(const int32_t aControllerIndex) {
  if (Contains(aControllerIndex)) {
    list[aControllerIndex].Reset();
  }
}

void
ControllerContainer::SetEnabled(const int32_t aControllerIndex, const bool aEnabled) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  list[aControllerIndex].enabled = aEnabled;
  if (!aEnabled) {
    SetVisible(aControllerIndex, false);
  }
}

void
ControllerContainer::SetVisible(const int32_t aControllerIndex, const bool aVisible) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = list[aControllerIndex];
  if (controller.transform) {
    root->ToggleChild(*controller.transform, aVisible);
  }
}

void
ControllerContainer::SetTransform(const int32_t aControllerIndex, const vrb::Matrix& aTransform) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = list[aControllerIndex];
  controller.transformMatrix = aTransform;
  if (controller.transform) {
    controller.transform->SetTransform(aTransform);
  }
}

void
ControllerContainer::SetButtonState(const int32_t aControllerIndex, const int32_t aWhichButton, const bool aPressed) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  if (aPressed) {
    list[aControllerIndex].buttonState |= aWhichButton;
  } else {
    list[aControllerIndex].buttonState &= ~aWhichButton;
  }
}

void
ControllerContainer::SetTouchPosition(const int32_t aControllerIndex, const float aTouchX, const float aTouchY) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = list[aControllerIndex];
  controller.touched = true;
  controller.touchX = aTouchX;
  controller.touchY = aTouchY;
}

void
ControllerContainer::EndTouch(const int32_t aControllerIndex) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  list[aControllerIndex].touched = false;
}

void
ControllerContainer::SetScrolledDelta(const int32_t aControllerIndex, const float aScrollDeltaX, const float aScrollDeltaY) {
  if (!Contains(aControllerIndex)) {
    return;
  }
  Controller& controller = list[aControllerIndex];
  controller.scrollDeltaX = aScrollDeltaX;
  controller.scrollDeltaY = aScrollDeltaY;
}

} // namespace

namespace crow {

struct BrowserWorld::State {
  BrowserWorldWeakPtr self;
  std::vector<WidgetPtr> widgets;
  SurfaceObserverPtr surfaceObserver;
  DeviceDelegatePtr device;
  bool paused;
  bool glInitialized;
  ContextPtr context;
  ContextWeak contextWeak;
  NodeFactoryObjPtr factory;
  ParserObjPtr parser;
  GroupPtr rootOpaqueParent;
  GroupPtr rootOpaque;
  GroupPtr rootTransparent;
  LightPtr light;
  ControllerContainerPtr controllers;
  CullVisitorPtr cullVisitor;
  DrawableListPtr drawListOpaque;
  DrawableListPtr drawListTransparent;
  CameraPtr leftCamera;
  CameraPtr rightCamera;
  float nearClip;
  float farClip;
  JNIEnv* env;
  jobject activity;
  GestureDelegateConstPtr gestures;
  ExternalVRPtr externalVR;
  ExternalBlitterPtr blitter;
  bool windowsInitialized;
  TransformPtr skybox;

  State() : paused(true), glInitialized(false), env(nullptr), nearClip(0.1f),
            farClip(100.0f), activity(nullptr),
            windowsInitialized(false) {
    context = Context::Create();
    contextWeak = context;
    factory = NodeFactoryObj::Create(contextWeak);
    parser = ParserObj::Create(contextWeak);
    parser->SetObserver(factory);
    rootOpaque = Group::Create(contextWeak);
    rootTransparent = Group::Create(contextWeak);
    light = Light::Create(contextWeak);
    rootOpaqueParent = Group::Create(contextWeak);
    rootOpaqueParent->AddNode(rootOpaque);
    rootOpaque->AddLight(light);
    rootTransparent->AddLight(light);
    cullVisitor = CullVisitor::Create(contextWeak);
    drawListOpaque = DrawableList::Create(contextWeak);
    drawListTransparent = DrawableList::Create(contextWeak);
    controllers = ControllerContainer::Create();
    controllers->context = contextWeak;
    controllers->root = Toggle::Create(contextWeak);
    externalVR = ExternalVR::Create();
    blitter = ExternalBlitter::Create(contextWeak);
  }

  void UpdateControllers();
  WidgetPtr GetWidget(int32_t aHandle) const;
  WidgetPtr FindWidget(const std::function<bool(const WidgetPtr&)>& aCondition) const;
};

void
BrowserWorld::State::UpdateControllers() {
  std::vector<Widget*> active;
  for (const WidgetPtr& widget: widgets) {
    widget->TogglePointer(false);
  }
  for (Controller& controller: controllers->list) {
    if (!controller.enabled || (controller.index < 0)) {
      continue;
    }
    vrb::Vector start = controller.transformMatrix.MultiplyPosition(vrb::Vector());
    vrb::Vector direction = controller.transformMatrix.MultiplyDirection(vrb::Vector(0.0f, 0.0f, -1.0f));
    WidgetPtr hitWidget;
    float hitDistance = farClip;
    vrb::Vector hitPoint;
    for (const WidgetPtr& widget: widgets) {
      vrb::Vector result;
      float distance = 0.0f;
      bool isInWidget = false;
      if (widget->TestControllerIntersection(start, direction, result, isInWidget, distance)) {
        if (isInWidget && (distance < hitDistance)) {
          hitWidget = widget;
          hitDistance = distance;
          hitPoint = result;
        }
      }
    }
    if (hitWidget) {
      active.push_back(hitWidget.get());
      float theX = 0.0f, theY = 0.0f;
      hitWidget->ConvertToWidgetCoordinates(hitPoint, theX, theY);
      const uint32_t handle = hitWidget->GetHandle();
      const bool pressed = controller.buttonState & ControllerDelegate::BUTTON_TRIGGER ||
                           controller.buttonState & ControllerDelegate::BUTTON_TOUCHPAD;
      const bool wasPressed = controller.lastButtonState & ControllerDelegate::BUTTON_TRIGGER ||
                              controller.lastButtonState & ControllerDelegate::BUTTON_TOUCHPAD;
      if ((controller.pointerX != theX) ||
          (controller.pointerY != theY) ||
          (controller.widget != handle) ||
          (pressed != wasPressed)) {
        VRBrowser::HandleMotionEvent(handle, controller.index, jboolean(pressed), theX, theY);
        controller.widget = handle;
        controller.pointerX = theX;
        controller.pointerY = theY;
      }
      if ((controller.scrollDeltaX != 0.0f) || controller.scrollDeltaY != 0.0f) {
        VRBrowser::HandleScrollEvent(controller.widget, controller.index,
                            controller.scrollDeltaX, controller.scrollDeltaY);
        controller.scrollDeltaX = 0.0f;
        controller.scrollDeltaY = 0.0f;
      }
      if (!pressed) {
        if (controller.touched) {
          if (!controller.wasTouched) {
            controller.wasTouched = controller.touched;
          } else {
            VRBrowser::HandleScrollEvent(controller.widget,
                                controller.index,
                                (controller.touchX - controller.lastTouchX) * kScrollFactor,
                                (controller.touchY - controller.lastTouchY) * kScrollFactor);
          }
          controller.lastTouchX = controller.touchX;
          controller.lastTouchY = controller.touchY;
        } else {
          controller.wasTouched = false;
          controller.lastTouchX = controller.lastTouchY = 0.0f;
        }
      }
    } else if (controller.widget) {
      VRBrowser::HandleMotionEvent(0, controller.index, JNI_FALSE, 0.0f, 0.0f);
      controller.widget = 0;
    }
  }
  for (Widget* widget: active) {
    widget->TogglePointer(true);
  }
  active.clear();
  if (gestures) {
    const int32_t gestureCount = gestures->GetGestureCount();
    for (int32_t count = 0; count < gestureCount; count++) {
      const GestureType type = gestures->GetGestureType(count);
      jint javaType = -1;
      if (type == GestureType::SwipeLeft) {
        javaType = GestureSwipeLeft;
      } else if (type == GestureType::SwipeRight) {
        javaType = GestureSwipeRight;
      }
      if (javaType >= 0) {
        VRBrowser::HandleGesture(javaType);
      }
    }
  }
}

WidgetPtr
BrowserWorld::State::GetWidget(int32_t aHandle) const {
  return FindWidget([=](const WidgetPtr& aWidget){
    return aWidget->GetHandle() == aHandle;
  });
}

WidgetPtr
BrowserWorld::State::FindWidget(const std::function<bool(const WidgetPtr&)>& aCondition) const {
  for (const WidgetPtr & widget: widgets) {
    if (aCondition(widget)) {
      return widget;
    }
  }
  return {};
}

BrowserWorldPtr
BrowserWorld::Create() {
  BrowserWorldPtr result = std::make_shared<vrb::ConcreteClass<BrowserWorld, BrowserWorld::State> >();
  result->m.self = result;
  result->m.surfaceObserver = std::make_shared<SurfaceObserver>(result->m.self);
  result->m.context->GetSurfaceTextureFactory()->AddGlobalObserver(result->m.surfaceObserver);
  return result;
}

vrb::ContextWeak
BrowserWorld::GetWeakContext() {
  return m.context;
}

void
BrowserWorld::RegisterDeviceDelegate(DeviceDelegatePtr aDelegate) {
  DeviceDelegatePtr previousDevice = std::move(m.device);
  m.device = aDelegate;
  if (m.device) {
    m.device->RegisterImmersiveDisplay(m.externalVR);
#if defined(SNAPDRAGONVR)
    m.device->SetClearColor(vrb::Color(0.0f, 0.0f, 0.0f));
#else
    m.device->SetClearColor(vrb::Color(0.15f, 0.15f, 0.15f));
#endif
    m.leftCamera = m.device->GetCamera(device::Eye::Left);
    m.rightCamera = m.device->GetCamera(device::Eye::Right);
    ControllerDelegatePtr delegate = m.controllers;
    m.device->SetClipPlanes(m.nearClip, m.farClip);
    m.device->SetControllerDelegate(delegate);
    m.gestures = m.device->GetGestureDelegate();
  } else if (previousDevice) {
    m.leftCamera = m.rightCamera = nullptr;
    for (Controller& controller: m.controllers->list) {
      if (controller.transform) {
        controller.transform->RemoveFromParents();
      }
      controller.Reset();

    }
    previousDevice->ReleaseControllerDelegate();
    m.gestures = nullptr;
  }
}

void
BrowserWorld::Pause() {
  m.paused = true;
}

void
BrowserWorld::Resume() {
  m.paused = false;
}

bool
BrowserWorld::IsPaused() const {
  return m.paused;
}

void
BrowserWorld::InitializeJava(JNIEnv* aEnv, jobject& aActivity, jobject& aAssetManager) {
  VRB_LOG("BrowserWorld::InitializeJava");
  if (m.context) {
    m.context->InitializeJava(aEnv, aActivity, aAssetManager);
  }
  m.env = aEnv;
  if (!m.env) {
    return;
  }
  m.activity = m.env->NewGlobalRef(aActivity);
  if (!m.activity) {
    return;
  }
  jclass clazz = m.env->GetObjectClass(m.activity);
  if (!clazz) {
    return;
  }

  VRBrowser::InitializeJava(m.env, m.activity);
  GeckoSurfaceTexture::InitializeJava(m.env, m.activity);

  VRBrowser::RegisterExternalContext((jlong)m.externalVR->GetSharedData());

  if (!m.controllers->modelsLoaded) {
    const int32_t modelCount = m.device->GetControllerModelCount();
    for (int32_t index = 0; index < modelCount; index++) {
      const std::string fileName = m.device->GetControllerModelName(index);
      if (!fileName.empty()) {
        m.controllers->SetUpModelsGroup(index);
        m.factory->SetModelRoot(m.controllers->models[index]);
        m.parser->LoadModel(fileName);
      }
    }
    m.rootOpaque->AddNode(m.controllers->root);
    CreateControllerPointer();
    m.skybox = CreateSkyBox("cubemap/space");
    m.rootOpaqueParent->AddNode(m.skybox);
    CreateFloor();
    m.controllers->modelsLoaded = true;
  }
}

void
BrowserWorld::InitializeGL() {
  VRB_LOG("BrowserWorld::InitializeGL");
  if (m.context) {
    if (!m.glInitialized) {
      m.glInitialized = m.context->InitializeGL();
      VRB_GL_CHECK(glEnable(GL_BLEND));
      VRB_GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
      VRB_GL_CHECK(glEnable(GL_DEPTH_TEST));
      VRB_GL_CHECK(glEnable(GL_CULL_FACE));
      if (!m.glInitialized) {
        return;
      }
      SurfaceTextureFactoryPtr factory = m.context->GetSurfaceTextureFactory();
      for (WidgetPtr& widget: m.widgets) {
        const std::string name = widget->GetSurfaceTextureName();
        jobject surface = factory->LookupSurfaceTexture(name);
        if (surface) {
          SetSurfaceTexture(name, surface);
        }
      }
    }
  }
}

void
BrowserWorld::ShutdownJava() {
  VRB_LOG("BrowserWorld::ShutdownJava");
  GeckoSurfaceTexture::ShutdownJava();
  VRBrowser::ShutdownJava();
  if (m.env) {
    m.env->DeleteGlobalRef(m.activity);
  }
  m.activity = nullptr;

  m.env = nullptr;
}

void
BrowserWorld::ShutdownGL() {
  VRB_LOG("BrowserWorld::ShutdownGL");
  if (m.context) {
    m.context->ShutdownGL();
  }
  m.glInitialized = false;
}

void
BrowserWorld::Draw() {
  if (!m.device) {
    VRB_LOG("No device");
    return;
  }
  if (m.paused) {
    VRB_LOG("BrowserWorld Paused");
    return;
  }
  if (!m.glInitialized) {
    m.glInitialized = m.context->InitializeGL();
    if (!m.glInitialized) {
      VRB_LOG("FAILED to initialize GL");
      return;
    }
  }

  m.device->ProcessEvents();
  m.context->Update();
  m.UpdateControllers();
  m.externalVR->PullBrowserState();
  if (m.externalVR->IsPresenting()) {
    DrawImmersive();
  } else {
    DrawWorld();
    m.externalVR->PushSystemState();
  }
  // Update the 3d audio engine with the most recent head rotation.
  const vrb::Matrix &head = m.device->GetHeadTransform();
  const vrb::Vector p = head.GetTranslation();
  const vrb::Quaternion q(head);
  VRBrowser::HandleAudioPose(q.x(), q.y(), q.z(), q.w(), p.x(), p.y(), p.z());

}

void
BrowserWorld::SetSurfaceTexture(const std::string& aName, jobject& aSurface) {
  VRB_LOG("SetSurfaceTexture: %s", aName.c_str());
  WidgetPtr widget = m.FindWidget([=](const WidgetPtr& aWidget) -> bool {
    return aName == aWidget->GetSurfaceTextureName();
  });
  if (widget) {
    int32_t width = 0, height = 0;
    widget->GetSurfaceTextureSize(width, height);
    VRBrowser::DispatchCreateWidget(widget->GetHandle(), aSurface, width, height);
  }
}

void
BrowserWorld::AddWidget(int32_t aHandle, const WidgetPlacement& aPlacement) {
  if (m.GetWidget(aHandle)) {
    VRB_LOG("Widget with handle %d already added, updating it.", aHandle);
    UpdateWidget(aHandle, aPlacement);
    return;
  }
  float worldWidth = aPlacement.worldWidth;
  if (worldWidth <= 0.0f) {
    worldWidth = aPlacement.width * kWorldDPIRatio;
  }

  WidgetPtr widget = Widget::Create(m.contextWeak,
                                    aHandle,
                                    (int32_t)(aPlacement.width * aPlacement.density),
                                    (int32_t)(aPlacement.height * aPlacement.density),
                                    worldWidth);
  if (aPlacement.opaque) {
    m.rootOpaque->AddNode(widget->GetRoot());
  } else {
    m.rootTransparent->AddNode(widget->GetRoot());
  }

  m.widgets.push_back(widget);
  UpdateWidget(widget->GetHandle(), aPlacement);

  if (!aPlacement.showPointer) {
    vrb::NodePtr emptyNode = vrb::Group::Create(m.contextWeak);
    widget->SetPointerGeometry(emptyNode);
  }
}

void
BrowserWorld::UpdateWidget(int32_t aHandle, const WidgetPlacement& aPlacement) {
  WidgetPtr widget = m.GetWidget(aHandle);
  if (!widget) {
      VRB_LOG("Can't find Widget with handle: %d", aHandle);
      return;
  }

  widget->ToggleWidget(aPlacement.visible);

  WidgetPtr parent = m.GetWidget(aPlacement.parentHandle);

  int32_t parentWidth = 0, parentHeight = 0;
  float parentWorldWith = 0.0f, parentWorldHeight = 0.0f;

  if (parent) {
    parent->GetSurfaceTextureSize(parentWidth, parentHeight);
    parent->GetWorldSize(parentWorldWith, parentWorldHeight);
  }

  float worldWidth = 0.0f, worldHeight = 0.0f;
  widget->GetWorldSize(worldWidth, worldHeight);

  float newWorldWidth = aPlacement.worldWidth;
  if (newWorldWidth <= 0.0f) {
    newWorldWidth = aPlacement.width * kWorldDPIRatio;
  }

  if (newWorldWidth != worldWidth) {
    widget->SetWorldWidth(newWorldWidth);
    widget->GetWorldSize(worldWidth, worldHeight);
  }

  vrb::Matrix transform = vrb::Matrix::Identity();
  if (aPlacement.rotationAxis.Magnitude() > std::numeric_limits<float>::epsilon()) {
    transform = vrb::Matrix::Rotation(aPlacement.rotationAxis, aPlacement.rotation);
  }

  vrb::Vector translation = vrb::Vector(aPlacement.translation.x() * kWorldDPIRatio,
                                        aPlacement.translation.y() * kWorldDPIRatio,
                                        aPlacement.translation.z() * kWorldDPIRatio);
  // Widget anchor point
  translation -= vrb::Vector((aPlacement.anchor.x() - 0.5f) * worldWidth,
                             aPlacement.anchor.y() * worldHeight,
                             0.0f);
  // Parent anchor point
  if (parent) {
    translation += vrb::Vector(
        parentWorldWith * aPlacement.parentAnchor.x() - parentWorldWith * 0.5f,
        parentWorldHeight * aPlacement.parentAnchor.y(),
        0.0f);
  }

  transform.TranslateInPlace(translation);
  widget->SetTransform(parent ? parent->GetTransform().PostMultiply(transform) : transform);
}

void
BrowserWorld::RemoveWidget(int32_t aHandle) {
  WidgetPtr widget = m.GetWidget(aHandle);
  if (widget) {
    widget->GetRoot()->RemoveFromParents();
    auto it = std::find(m.widgets.begin(), m.widgets.end(), widget);
    if (it != m.widgets.end()) {
      m.widgets.erase(it);
    }
  }
}

JNIEnv*
BrowserWorld::GetJNIEnv() const {
  return m.env;
}

BrowserWorld::BrowserWorld(State& aState) : m(aState) {
  sWorld = this;
}

BrowserWorld::~BrowserWorld() {
  if (sWorld == this) {
    sWorld = nullptr;
  }
}

void
BrowserWorld::DrawWorld() {
  m.device->SetRenderMode(device::RenderMode::StandAlone);
  vrb::Vector headPosition = m.device->GetHeadTransform().GetTranslation();
  m.skybox->SetTransform(vrb::Matrix::Translation(headPosition));
  m.rootTransparent->SortNodes([=](const NodePtr& a, const NodePtr& b) {
    return DistanceToNode(a, headPosition) < DistanceToNode(b, headPosition);
  });
  m.drawListOpaque->Reset();
  m.drawListTransparent->Reset();
  m.rootOpaqueParent->Cull(*m.cullVisitor, *m.drawListOpaque);
  m.rootTransparent->Cull(*m.cullVisitor, *m.drawListTransparent);
  m.device->StartFrame();
  m.device->BindEye(device::Eye::Left);
  m.drawListOpaque->Draw(*m.leftCamera);
  VRB_GL_CHECK(glDepthMask(GL_FALSE));
  m.drawListTransparent->Draw(*m.leftCamera);
  VRB_GL_CHECK(glDepthMask(GL_TRUE));
  // When running the noapi flavor, we only want to render one eye.
#if !defined(VRBROWSER_NO_VR_API)
  m.device->BindEye(device::Eye::Right);
  m.drawListOpaque->Draw(*m.rightCamera);
  VRB_GL_CHECK(glDepthMask(GL_FALSE));
  m.drawListTransparent->Draw(*m.rightCamera);
  VRB_GL_CHECK(glDepthMask(GL_TRUE));
#endif // !defined(VRBROWSER_NO_VR_API)
  m.device->EndFrame();
}

void
BrowserWorld::DrawImmersive() {
  if (m.externalVR->IsFirstPresentingFrame()) {
    VRBrowser::PauseCompositor();
  }
  m.device->SetRenderMode(device::RenderMode::Immersive);
  /*
  float r = (float)rand() / (float)RAND_MAX;
  float g = (float)rand() / (float)RAND_MAX;
  float b = (float)rand() / (float)RAND_MAX;

  m.device->SetClearColor(vrb::Color(r, g, b));
   */
  m.device->StartFrame();
  VRB_GL_CHECK(glDepthMask(GL_FALSE));
  m.externalVR->RequestFrame(m.device->GetHeadTransform());
  int32_t surfaceHandle = 0;
  device::EyeRect leftEye, rightEye;
  m.externalVR->GetFrameResult(surfaceHandle, leftEye, rightEye);
  m.blitter->Update(surfaceHandle, leftEye, rightEye);
  m.device->BindEye(device::Eye::Left);
  m.blitter->Draw(device::Eye::Left);
#if !defined(VRBROWSER_NO_VR_API)
  m.device->BindEye(device::Eye::Right);
  m.blitter->Draw(device::Eye::Right);
#endif // !defined(VRBROWSER_NO_VR_API)
  m.device->EndFrame();
  m.blitter->Finish();
}

vrb::TransformPtr
BrowserWorld::CreateSkyBox(const std::string& basePath) {
  std::array<GLfloat, 24> cubeVertices {
    -1.0f,  1.0f,  1.0f, // 0
    -1.0f, -1.0f,  1.0f, // 1
     1.0f, -1.0f,  1.0f, // 2
     1.0f,  1.0f,  1.0f, // 3
    -1.0f,  1.0f, -1.0f, // 4
    -1.0f, -1.0f, -1.0f, // 5
     1.0f, -1.0f, -1.0f, // 6
     1.0f,  1.0f, -1.0f, // 7
  };

  std::array<GLushort, 24> cubeIndices {
      0, 1, 2, 3,
      3, 2, 6, 7,
      7, 6, 5, 4,
      4, 5, 1, 0,
      0, 3, 7, 4,
      1, 5, 6, 2
  };

  VertexArrayPtr array = VertexArray::Create(m.contextWeak);
  const float kLength = 50.0f;
  for (int i = 0; i < cubeVertices.size(); i += 3) {
    array->AppendVertex(Vector(-kLength * cubeVertices[i], -kLength * cubeVertices[i + 1], -kLength * cubeVertices[i + 2]));
    array->AppendUV(Vector(-kLength * cubeVertices[i], -kLength * cubeVertices[i + 1], -kLength * cubeVertices[i + 2]));
  }

  vrb::GeometryPtr geometry = vrb::Geometry::Create(m.contextWeak);
  geometry->SetVertexArray(array);


  for (int i = 0; i < cubeIndices.size(); i += 4) {
    std::vector<int> indices = {cubeIndices[i] + 1, cubeIndices[i + 1] + 1, cubeIndices[i + 2] + 1, cubeIndices[i + 3] + 1};
    geometry->AddFace(indices, indices, {});
  }

  RenderStatePtr state = RenderState::Create(m.contextWeak);
  TextureCubeMapPtr cubemap = vrb::TextureCubeMap::Create(m.contextWeak);
  cubemap->SetTextureParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  cubemap->SetTextureParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  cubemap->SetTextureParameter(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  cubemap->SetTextureParameter(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  cubemap->SetTextureParameter(GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  state->SetTexture(cubemap);

  auto path = [&](const std::string& name) { return basePath + "/" + name + ".jpg"; };
  vrb::TextureCubeMap::Load(m.contextWeak, cubemap, path("posx"), path("negx"), path("posy"), path("negy"), path("posz"), path("negz"));

  state->SetMaterial(Color(1.0f, 1.0f, 1.0f), Color(1.0f, 1.0f, 1.0f), Color(0.0f, 0.0f, 0.0f), 0.0f);
  geometry->SetRenderState(state);
  vrb::TransformPtr transform = vrb::Transform::Create(m.contextWeak);
  transform->AddNode(geometry);
  transform->SetTransform(Matrix::Position(vrb::Vector(0.0f, 0.0f, 0.0f)));
  return transform;
}


void
BrowserWorld::CreateFloor() {
  vrb::TransformPtr model = Transform::Create(m.contextWeak);
  m.factory->SetModelRoot(model);
  m.parser->LoadModel("FirefoxPlatform2_low.obj");
  m.rootOpaque->AddNode(model);
  vrb::Matrix transform = vrb::Matrix::Identity();
  transform.ScaleInPlace(Vector(40.0, 40.0, 40.0));
  transform.TranslateInPlace(Vector(0.0, -2.5f, 1.0));
  transform.PostMultiplyInPlace(vrb::Matrix::Rotation(Vector(1.0, 0.0, 0.0), float(M_PI * 0.5)));
  model->SetTransform(transform);
}


void
BrowserWorld::CreateControllerPointer() {
  if (m.controllers->pointerModel) {
    return;
  }
  VertexArrayPtr array = VertexArray::Create(m.contextWeak);
  const float kLength = -5.0f;
  const float kHeight = 0.0008f;

  array->AppendVertex(Vector(-kHeight, -kHeight, 0.0f)); // Bottom left
  array->AppendVertex(Vector(kHeight, -kHeight, 0.0f)); // Bottom right
  array->AppendVertex(Vector(kHeight, kHeight, 0.0f)); // Top right
  array->AppendVertex(Vector(-kHeight, kHeight, 0.0f)); // Top left
  array->AppendVertex(Vector(0.0f, 0.0f, kLength)); // Tip

  array->AppendNormal(Vector(-1.0f, -1.0f, 0.0f).Normalize()); // Bottom left
  array->AppendNormal(Vector(1.0f, -1.0f, 0.0f).Normalize()); // Bottom right
  array->AppendNormal(Vector(1.0f, 1.0f, 0.0f).Normalize()); // Top right
  array->AppendNormal(Vector(-1.0f, 1.0f, 0.0f).Normalize()); // Top left
  array->AppendNormal(Vector(0.0f, 0.0f, -1.0f).Normalize()); // in to the screen


  RenderStatePtr state = RenderState::Create(m.contextWeak);
  state->SetMaterial(Color(0.6f, 0.0f, 0.0f), Color(1.0f, 0.0f, 0.0f), Color(0.5f, 0.5f, 0.5f),
                     96.078431f);
  GeometryPtr geometry = Geometry::Create(m.contextWeak);
  geometry->SetVertexArray(array);
  geometry->SetRenderState(state);

  std::vector<int> index;
  std::vector<int> uvIndex;

  index.push_back(1);
  index.push_back(2);
  index.push_back(5);
  geometry->AddFace(index, uvIndex, index);

  index.clear();
  index.push_back(2);
  index.push_back(3);
  index.push_back(5);
  geometry->AddFace(index, uvIndex, index);

  index.clear();
  index.push_back(3);
  index.push_back(4);
  index.push_back(5);
  geometry->AddFace(index, uvIndex, index);

  index.clear();
  index.push_back(4);
  index.push_back(1);
  index.push_back(5);
  geometry->AddFace(index, uvIndex, index);

  m.controllers->pointerModel = std::move(geometry);
  for (Controller& controller: m.controllers->list) {
    if (controller.transform) {
      controller.transform->AddNode(m.controllers->pointerModel);
    }
  }
}

float
BrowserWorld::DistanceToNode(const vrb::NodePtr& aTargetNode, const vrb::Vector& aPosition) const {
  float result = -1;
  Node::Traverse(aTargetNode, [&](const NodePtr &aNode, const GroupPtr &aTraversingFrom) {
    vrb::TransformPtr transform = std::dynamic_pointer_cast<vrb::Transform>(aNode);
    if (transform) {
      vrb::Vector targetPos = transform->GetTransform().GetTranslation();
      result = (targetPos - aPosition).Magnitude();
      return true;
    }
    return false;
  });

  return result;
}

} // namespace crow


#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
    Java_org_mozilla_vrbrowser_VRBrowserActivity_##method_name

extern "C" {

JNI_METHOD(void, addWidgetNative)
(JNIEnv* aEnv, jobject, jint aHandle, jobject aPlacement) {
  crow::WidgetPlacementPtr placement = crow::WidgetPlacement::FromJava(aEnv, aPlacement);
  if (placement && sWorld) {
    sWorld->AddWidget(aHandle, *placement);
  }
}

JNI_METHOD(void, updateWidgetNative)
(JNIEnv* aEnv, jobject, jint aHandle, jobject aPlacement) {
  crow::WidgetPlacementPtr placement = crow::WidgetPlacement::FromJava(aEnv, aPlacement);
  if (placement) {
    sWorld->UpdateWidget(aHandle, *placement);
  }
}

JNI_METHOD(void, removeWidgetNative)
(JNIEnv*, jobject, jint aHandle) {
  if (sWorld) {
    sWorld->RemoveWidget(aHandle);
  }
}

} // extern "C"
