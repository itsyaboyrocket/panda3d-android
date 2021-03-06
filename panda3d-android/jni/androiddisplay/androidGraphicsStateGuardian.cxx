// Filename: AndroidGraphicsStateGuardian.cxx
// Created by:  pro-rsoft (21May09)
//
////////////////////////////////////////////////////////////////////
//
// PANDA 3D SOFTWARE
// Copyright (c) Carnegie Mellon University.  All rights reserved.
//
// All use of this software is subject to the terms of the revised BSD
// license.  You should have received a copy of this license along
// with this source code in a file named "LICENSE."
//
////////////////////////////////////////////////////////////////////

#include "androidGraphicsStateGuardian.h"
#include "config_androiddisplay.h"
#include "lightReMutexHolder.h"

#include <dlfcn.h>

TypeHandle AndroidGraphicsStateGuardian::_type_handle;

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::Constructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
AndroidGraphicsStateGuardian::
AndroidGraphicsStateGuardian(GraphicsEngine *engine, GraphicsPipe *pipe,
       AndroidGraphicsStateGuardian *share_with) :
#ifdef OPENGLES_2
  GLES2GraphicsStateGuardian(engine, pipe)
#else
  GLESGraphicsStateGuardian(engine, pipe)
#endif
{
  _share_context=0;
  _context=0;
  _egl_display=0;
  _fbconfig=0;

  if (share_with != (AndroidGraphicsStateGuardian *)NULL) {
    _prepared_objects = share_with->get_prepared_objects();
    _share_context = share_with->_context;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::Destructor
//       Access: Public
//  Description:
////////////////////////////////////////////////////////////////////
AndroidGraphicsStateGuardian::
~AndroidGraphicsStateGuardian() {
  if (_context != (EGLContext)NULL) {
    if (!eglDestroyContext(_egl_display, _context)) {
      androidegldisplay_cat.error() << "Failed to destroy EGL context: "
        << get_egl_error_string(eglGetError()) << "\n";
    }
    _context = (EGLContext)NULL;
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::get_properties
//       Access: Private
//  Description: Gets the FrameBufferProperties to match the
//               indicated config.
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
get_properties(FrameBufferProperties &properties,
      bool &pbuffer_supported, bool &pixmap_supported,
                        bool &slow, EGLConfig config) {

  properties.clear();

  // Now update our framebuffer_mode and bit depth appropriately.
  EGLint red_size, green_size, blue_size,
    alpha_size,
    depth_size, stencil_size, samples, surface_type, caveat;

  eglGetConfigAttrib(_egl_display, config, EGL_RED_SIZE, &red_size);
  eglGetConfigAttrib(_egl_display, config, EGL_GREEN_SIZE, &green_size);
  eglGetConfigAttrib(_egl_display, config, EGL_BLUE_SIZE, &blue_size);
  eglGetConfigAttrib(_egl_display, config, EGL_ALPHA_SIZE, &alpha_size);
  eglGetConfigAttrib(_egl_display, config, EGL_DEPTH_SIZE, &depth_size);
  eglGetConfigAttrib(_egl_display, config, EGL_STENCIL_SIZE, &stencil_size);
  eglGetConfigAttrib(_egl_display, config, EGL_SAMPLES, &samples);
  eglGetConfigAttrib(_egl_display, config, EGL_SURFACE_TYPE, &surface_type);
  eglGetConfigAttrib(_egl_display, config, EGL_CONFIG_CAVEAT, &caveat);
  int err = eglGetError();
  if (err != EGL_SUCCESS) {
    androidegldisplay_cat.error() << "Failed to get EGL config attrib: "
      << get_egl_error_string(err) << "\n";
  }

  pbuffer_supported = false;
  if ((surface_type & EGL_PBUFFER_BIT)!=0) {
    pbuffer_supported = true;
  }

  pixmap_supported = false;
  if ((surface_type & EGL_PIXMAP_BIT)!=0) {
    pixmap_supported = true;
  }

  slow = false;
  if (caveat == EGL_SLOW_CONFIG) {
    slow = true;
  }

  if ((surface_type & EGL_WINDOW_BIT)==0) {
    // We insist on having a context that will support an onscreen window.
    return;
  }

  properties.set_back_buffers(1);
  properties.set_rgb_color(1);
  properties.set_color_bits(red_size+green_size+blue_size);
  properties.set_stencil_bits(stencil_size);
  properties.set_depth_bits(depth_size);
  properties.set_alpha_bits(alpha_size);
  properties.set_multisamples(samples);

  // Set both hardware and software bits, indicating not-yet-known.
  properties.set_force_software(1);
  properties.set_force_hardware(1);
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::choose_pixel_format
//       Access: Private
//  Description: Selects a visual or fbconfig for all the windows
//               and buffers that use this gsg.  Also creates the GL
//               context and obtains the visual.
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
choose_pixel_format(const FrameBufferProperties &properties,
        bool need_pbuffer, bool need_pixmap) {

  AndroidGraphicsPipe *android_pipe;
  DCAST_INTO_V(android_pipe, get_pipe());

  _egl_display = 0;
  _context = 0;
  _fbconfig = 0;
  _fbprops.clear();



//  int attrib_list[] = {
//#ifdef OPENGLES_1
//    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
//#endif
//#ifdef OPENGLES_2
//    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
//#endif
//    EGL_SURFACE_TYPE, EGL_DONT_CARE,
//    EGL_NONE
//  };
//
//  // First get the number of matching configurations, so we know how much memory to allocate.
//  int num_configs = 0, returned_configs;
//  if (!eglChooseConfig(_egl_display, attrib_list, NULL, num_configs, &returned_configs) || returned_configs <= 0) {
//    androidegldisplay_cat.error() << "eglChooseConfig failed: "
//      << get_egl_error_string(eglGetError()) << "\n";
//    return;
//  }
//
//  num_configs = returned_configs;
//  EGLConfig *configs = new EGLConfig[num_configs];
//
//  if (!eglChooseConfig(_egl_display, attrib_list, configs, num_configs, &returned_configs) || returned_configs <= 0) {
//    androidegldisplay_cat.error() << "eglChooseConfig failed: "
//      << get_egl_error_string(eglGetError()) << "\n";
//    delete[] configs;
//    return;
//  }

//  // initialize OpenGL ES and EGL
//
//  AndroidGraphicsPipe *android_pipe = (AndroidGraphicsPipe *) get_pipe();
//  //DCAST_INTO_R(android_pipe, android_pipe, NULL);
//
//  /*
//   * Here specify the attributes of the desired configuration.
//   * Below, we select an EGLConfig with at least 8 bits per color
//   * component compatible with on-screen windows
//   */
//  const EGLint attribs[] = {
//          EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
//          EGL_BLUE_SIZE, 8,
//          EGL_GREEN_SIZE, 8,
//          EGL_RED_SIZE, 8,
//          EGL_NONE
//  };
//  EGLint w, h, dummy, format;
//  EGLint numConfigs;
//
//  EGLSurface surface;
//  EGLContext context;
//
//  //  /* Here, the application chooses the configuration it desires. In this
//  //   * sample, we have a very simplified selection process, where we pick
//  //   * the first EGLConfig that matches our criteria */
//  eglChooseConfig(_egl_display, attribs, &_fbconfig, 1, &numConfigs);
//  //
//  //  /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
//  //   * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
//  //   * As soon as we picked a EGLConfig, we can safely reconfigure the
//  //   * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
//
//  eglGetConfigAttrib(_egl_display, _fbconfig, EGL_NATIVE_VISUAL_ID, &format);
//  ANativeWindow_setBuffersGeometry(android_pipe->get_android_app()->window, 0, 0, format);

//  int best_quality = 0;
//  int best_result = 0;
//  FrameBufferProperties best_props;
//
//  for (int i = 0; i < num_configs; ++i) {
//    FrameBufferProperties fbprops;
//    bool pbuffer_supported, pixmap_supported, slow;
//    get_properties(fbprops, pbuffer_supported, pixmap_supported,
//                   slow, configs[i]);
//    // We're not protecting this code by an is_debug() check, because if we do,
//    // some weird compiler bug appears and somehow makes the quality always 0.
//    const char *pbuffertext = pbuffer_supported ? " (pbuffer)" : "";
//    const char *pixmaptext = pixmap_supported ? " (pixmap)" : "";
//    const char *slowtext = slow ? " (slow)" : "";
//    androidegldisplay_cat.debug()
//      << i << ": " << fbprops << pbuffertext << pixmaptext << slowtext << "\n";
//    int quality = fbprops.get_quality(properties);
//    if ((quality > 0)&&(slow)) quality -= 10000000;
//
//    if (need_pbuffer && !pbuffer_supported) {
//      continue;
//    }
//    if (need_pixmap && !pixmap_supported) {
//      continue;
//    }
//
//    if (quality > best_quality) {
//      best_quality = quality;
//      best_result = i;
//      best_props = fbprops;
//    }
//  }
//
//  if (best_quality > 0) {
//    androidegldisplay_cat.debug()
//      << "Chosen config " << best_result << ": " << best_props << "\n";
//    _fbconfig = configs[best_result];
//#ifdef OPENGLES_2
//    EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
//    _context = eglCreateContext(_egl_display, _fbconfig, _share_context, context_attribs);
//#else
//    _context = eglCreateContext(_egl_display, _fbconfig, _share_context, NULL);
//#endif
//    int err = eglGetError();
//    if (_context && err == EGL_SUCCESS) {
////      if (_visual) {
//        _fbprops = best_props;
//        delete[] configs;
//        return;
////      }
//    }
//    // This really shouldn't happen, so I'm not too careful about cleanup.
//    androidegldisplay_cat.error()
//      << "Could not create EGL context!\n"
//      << get_egl_error_string(err) << "\n";
//    _fbconfig = 0;
//    _context = 0;
//  }
//
//  androidegldisplay_cat.error() <<
//    "Could not find a usable pixel format.\n";
//
//  delete[] configs;
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::reset
//       Access: Public, Virtual
//  Description: Resets all internal state as if the gsg were newly
//               created.
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
reset() {
#ifdef OPENGLES_2
  GLES2GraphicsStateGuardian::reset();
#else
  GLESGraphicsStateGuardian::reset();
#endif

  // If "Mesa" is present, assume software.  However, if "Mesa DRI" is
  // found, it's actually a Mesa-based OpenGL layer running over a
  // hardware driver.
  if (_gl_renderer.find("Mesa") != string::npos &&
      _gl_renderer.find("Mesa DRI") == string::npos) {
    // It's Mesa, therefore probably a software context.
    _fbprops.set_force_software(1);
    _fbprops.set_force_hardware(0);
  } else {
    _fbprops.set_force_hardware(1);
    _fbprops.set_force_software(0);
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::egl_is_at_least_version
//       Access: Public
//  Description: Returns true if the runtime GLX version number is at
//               least the indicated value, false otherwise.
////////////////////////////////////////////////////////////////////
bool AndroidGraphicsStateGuardian::
egl_is_at_least_version(int major_version, int minor_version) const {
  if (_egl_version_major < major_version) {
    return false;
  }
  if (_egl_version_minor < minor_version) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::gl_flush
//       Access: Protected, Virtual
//  Description: Calls glFlush().
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
gl_flush() const {
#ifdef OPENGLES_2
  GLES2GraphicsStateGuardian::gl_flush();
#else
  GLESGraphicsStateGuardian::gl_flush();
#endif
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::gl_get_error
//       Access: Protected, Virtual
//  Description: Returns the result of glGetError().
////////////////////////////////////////////////////////////////////
GLenum AndroidGraphicsStateGuardian::
gl_get_error() const {
#ifdef OPENGLES_2
  return GLES2GraphicsStateGuardian::gl_get_error();
#else
  return GLESGraphicsStateGuardian::gl_get_error();
#endif
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::query_gl_version
//       Access: Protected, Virtual
//  Description: Queries the runtime version of OpenGL in use.
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
query_gl_version() {
#ifdef OPENGLES_2
  GLES2GraphicsStateGuardian::query_gl_version();
#else
  GLESGraphicsStateGuardian::query_gl_version();
#endif

  // Calling eglInitialize on an already-initialized display will
  // just provide us the version numbers.
  if (!eglInitialize(_egl_display, &_egl_version_major, &_egl_version_minor)) {
    androidegldisplay_cat.error() << "Failed to get EGL version number: "
      << get_egl_error_string(eglGetError()) << "\n";
  }

  // We output to glesgsg_cat instead of androidegldisplay_cat, since this is
  // where the GL version has been output, and it's nice to see the
  // two of these together.
#ifdef OPENGLES_2
  if (gles2gsg_cat.is_debug()) {
    gles2gsg_cat.debug()
#else
  if (glesgsg_cat.is_debug()) {
    glesgsg_cat.debug()
#endif
      << "EGL_VERSION = " << _egl_version_major << "." << _egl_version_minor
      << "\n";
  }
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::get_extra_extensions
//       Access: Protected, Virtual
//  Description: This may be redefined by a derived class (e.g. glx or
//               wgl) to get whatever further extensions strings may
//               be appropriate to that interface, in addition to the
//               GL extension strings return by glGetString().
////////////////////////////////////////////////////////////////////
void AndroidGraphicsStateGuardian::
get_extra_extensions() {
  save_extensions(eglQueryString(_egl_display, EGL_EXTENSIONS));
}

////////////////////////////////////////////////////////////////////
//     Function: AndroidGraphicsStateGuardian::do_get_extension_func
//       Access: Public, Virtual
//  Description: Returns the pointer to the GL extension function with
//               the indicated name.  It is the responsibility of the
//               caller to ensure that the required extension is
//               defined in the OpenGL runtime prior to calling this;
//               it is an error to call this for a function that is
//               not defined.
////////////////////////////////////////////////////////////////////
void *AndroidGraphicsStateGuardian::
do_get_extension_func(const char *prefix, const char *name) {
  string fullname = string(prefix) + string(name);

  return (void *)eglGetProcAddress(fullname.c_str());
}
