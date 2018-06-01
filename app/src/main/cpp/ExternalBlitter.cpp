/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalBlitter.h"
#include "GeckoSurfaceTexture.h"
#include "vrb/ConcreteClass.h"
#include "vrb/private/ResourceGLState.h"
#include "vrb/gl.h"
#include "vrb/GLError.h"
#include "vrb/Logger.h"
#include "vrb/ShaderUtil.h"

#include <map>

namespace {
static const char* sVertexShader = R"SHADER(
attribute vec4 a_position;
attribute vec2 a_uv;
varying vec2 v_uv;
void main(void) {
  v_uv = a_uv;
  gl_Position = a_position;
}
)SHADER";

static const char* sFragmentShader = R"SHADER(
#extension GL_OES_EGL_image_external : require
precision mediump float;

uniform samplerExternalOES u_texture0;

varying vec2 v_uv;

void main() {
  gl_FragColor = texture2D(u_texture0, v_uv);
}
)SHADER";

static const GLfloat sVerticies[] = {
    -1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f,
    1.0f, 1.0f, 0.0f,
    1.0f, -1.0f, 0.0f
};

}

namespace crow {

struct ExternalBlitter::State : public vrb::ResourceGL::State {
  GLuint vertexShader;
  GLuint fragmentShader;
  GLuint program;
  GLint aPosition;
  GLint aUV;
  GLint uTexture0;
  device::EyeRect eyes[device::EyeCount];
  GeckoSurfaceTexturePtr surface;
  GLfloat leftUV[8];
  GLfloat rightUV[8];
  std::map<const int32_t, GeckoSurfaceTexturePtr> surfaceMap;
  State()
      : vertexShader(0)
      , fragmentShader(0)
      , program(0)
      , aPosition(0)
      , aUV(0)
      , uTexture0(0)
      , leftUV{0.0f, 0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 0.5f, 1.0f}
      , rightUV{0.5f, 0.0f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f}
  {}
};

ExternalBlitterPtr
ExternalBlitter::Create(vrb::ContextWeak& aContext) {
  return std::make_shared<vrb::ConcreteClass<ExternalBlitter, ExternalBlitter::State> >(aContext);
}

void
ExternalBlitter::Update(const int32_t aSurfaceHandle, const device::EyeRect& aLeftEye,
                        const device::EyeRect& aRightEye) {
  if (m.surface) {
    m.surface->ReleaseTexImage();
    m.surface = nullptr;
  }
  std::map<const int32_t, GeckoSurfaceTexturePtr>::iterator iter = m.surfaceMap.find(aSurfaceHandle);

  if (iter == m.surfaceMap.end()) {
    VRB_LOG("Creating GeckoSurfaceTexture for handle: %d", aSurfaceHandle);
    m.surface = GeckoSurfaceTexture::Create(aSurfaceHandle);
    m.surface->AttachToGLContext(eglGetCurrentContext());
    m.surfaceMap[aSurfaceHandle] = m.surface;
  } else {
    m.surface = iter->second;
  }

  if (!m.surface) {
    VRB_LOG("Failed to find GeckoSurfaceTexture for handle: %d", aSurfaceHandle);
    return;
  }
  //m.surface->DetachFromGLContext();
  m.surface->UpdateTexImage();
  m.eyes[device::EyeIndex(device::Eye::Left)] = aLeftEye;
  m.eyes[device::EyeIndex(device::Eye::Right)] = aRightEye;
}

void
ExternalBlitter::Draw(const device::Eye aEye) {
  if (!m.program || !m.surface) {
    VRB_LOG("ExternalBlitter::Draw FAILED!");
    return;
  }
  const GLboolean enabled = glIsEnabled(GL_DEPTH_TEST);
  if (enabled) {
    VRB_GL_CHECK(glDisable(GL_DEPTH_TEST));
  }
  VRB_GL_CHECK(glUseProgram(m.program));
  VRB_GL_CHECK(glActiveTexture(GL_TEXTURE0));
  VRB_GL_CHECK(glBindTexture(GL_TEXTURE_EXTERNAL_OES, m.surface->GetTextureName()));
  //m.defaultT->Bind();
  VRB_GL_CHECK(glUniform1i(m.uTexture0, 0));
  VRB_GL_CHECK(glVertexAttribPointer((GLuint)m.aPosition, 3, GL_FLOAT, GL_FALSE, 0, sVerticies));
  VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)m.aPosition));
  GLfloat* data = (aEye == device::Eye::Left ? &m.leftUV[0] : &m.rightUV[0]);
  VRB_GL_CHECK(glVertexAttribPointer((GLuint)m.aUV, 2, GL_FLOAT, GL_FALSE, 0, data));
  VRB_GL_CHECK(glEnableVertexAttribArray((GLuint)m.aUV));
  VRB_GL_CHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
  if (enabled) {
    VRB_GL_CHECK(glEnable(GL_DEPTH_TEST));
  }
}

void
ExternalBlitter::Finish() {
  //if (m.surface) {
  //  m.surface->ReleaseTexImage();
  //  m.surface = nullptr;
  //}
}

ExternalBlitter::ExternalBlitter(State& aState, vrb::ContextWeak& aContext)
    : vrb::ResourceGL(aState, aContext)
    , m(aState)
{}

ExternalBlitter::~ExternalBlitter() {}

void
ExternalBlitter::InitializeGL(vrb::Context& aContext) {
  m.vertexShader = vrb::LoadShader(GL_VERTEX_SHADER, sVertexShader);
  m.fragmentShader = vrb::LoadShader(GL_FRAGMENT_SHADER, sFragmentShader);
  if (m.vertexShader && m.fragmentShader) {
    m.program = vrb::CreateProgram(m.vertexShader, m.fragmentShader);
  }
  if (m.program) {
    m.aPosition = vrb::GetAttributeLocation(m.program, "a_position");
    m.aUV = vrb::GetAttributeLocation(m.program, "a_uv");
    m.uTexture0 = vrb::GetUniformLocation(m.program, "u_texture0");
  }
}

void
ExternalBlitter::ShutdownGL(vrb::Context& aContext) {
  if (m.program) {
    VRB_GL_CHECK(glDeleteProgram(m.program));
    m.program = 0;
  }
  if (m.vertexShader) {
    VRB_GL_CHECK(glDeleteShader(m.vertexShader));
    m.vertexShader = 0;
  }
  if (m.vertexShader) {
    VRB_GL_CHECK(glDeleteShader(m.fragmentShader));
    m.fragmentShader = 0;
  }
}

} // namespace crow
