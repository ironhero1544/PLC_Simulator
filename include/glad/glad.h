#pragma once

#include <glad/gl.h>

// Preserve the GLAD 0.x API name used by the current codebase.
using GLADloadproc = GLADloadfunc;

#ifndef gladLoadGLLoader
#define gladLoadGLLoader gladLoadGL
#endif
