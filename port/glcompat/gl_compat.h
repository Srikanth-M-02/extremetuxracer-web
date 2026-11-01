/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Fixed-function OpenGL 1.x compatibility layer on top of OpenGL ES 3.0 /
WebGL 2.

ETR was written against the legacy fixed-function pipeline (immediate
mode, matrix stacks, client-side vertex arrays, lighting, fog, texture
coordinate generation, alpha test). None of that exists in WebGL, so this
header provides the subset of the legacy API that the game uses and
implements it with a single "uber" shader (see gl_compat.cpp).

Game code includes this header instead of <GL/gl.h> and <GL/glu.h>.
Functions that also exist in GLES 3 but need to be intercepted (glEnable,
glDrawArrays, ...) are redirected to glc* implementations via macros.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#ifndef ETR_GL_COMPAT_H
#define ETR_GL_COMPAT_H

#include <GLES3/gl3.h>

typedef double GLdouble;
typedef double GLclampd;

// ---------------------------------------------------------------------
// Legacy enums that are not part of GLES 3 (values from desktop GL)
// ---------------------------------------------------------------------
#define GL_QUADS                          0x0007
#define GL_QUAD_STRIP                     0x0008
#define GL_POLYGON                        0x0009

#define GL_CURRENT_COLOR                  0x0B00
#define GL_LIGHTING                       0x0B50
#define GL_LIGHT_MODEL_LOCAL_VIEWER       0x0B51
#define GL_LIGHT_MODEL_TWO_SIDE           0x0B52
#define GL_LIGHT_MODEL_AMBIENT            0x0B53
#define GL_SHADE_MODEL                    0x0B54
#define GL_COLOR_MATERIAL                 0x0B57
#define GL_FOG                            0x0B60
#define GL_FOG_INDEX                      0x0B61
#define GL_FOG_DENSITY                    0x0B62
#define GL_FOG_START                      0x0B63
#define GL_FOG_END                        0x0B64
#define GL_FOG_MODE                       0x0B65
#define GL_FOG_COLOR                      0x0B66
#define GL_NORMALIZE                      0x0BA1
#define GL_MODELVIEW_MATRIX               0x0BA6
#define GL_PROJECTION_MATRIX              0x0BA7
#define GL_ALPHA_TEST                     0x0BC0
#define GL_PERSPECTIVE_CORRECTION_HINT    0x0C50
#define GL_FOG_HINT                       0x0C54
#define GL_TEXTURE_GEN_S                  0x0C60
#define GL_TEXTURE_GEN_T                  0x0C61
#define GL_TEXTURE_GEN_R                  0x0C62
#define GL_TEXTURE_GEN_Q                  0x0C63
#define GL_DOUBLEBUFFER                   0x0C32
#define GL_MAX_LIGHTS                     0x0D31
#define GL_MAX_MODELVIEW_STACK_DEPTH      0x0D36
#define GL_MAX_PROJECTION_STACK_DEPTH     0x0D38
#define GL_TEXTURE_WIDTH                  0x1000
#define GL_TEXTURE_HEIGHT                 0x1001
#define GL_DOUBLE                         0x140A

#define GL_AMBIENT                        0x1200
#define GL_DIFFUSE                        0x1201
#define GL_SPECULAR                       0x1202
#define GL_POSITION                       0x1203
#define GL_SPOT_DIRECTION                 0x1204
#define GL_SPOT_EXPONENT                  0x1205
#define GL_SPOT_CUTOFF                    0x1206
#define GL_CONSTANT_ATTENUATION           0x1207
#define GL_LINEAR_ATTENUATION             0x1208
#define GL_QUADRATIC_ATTENUATION          0x1209
#define GL_EMISSION                       0x1600
#define GL_SHININESS                      0x1601
#define GL_AMBIENT_AND_DIFFUSE            0x1602

#define GL_MODELVIEW                      0x1700
#define GL_PROJECTION                     0x1701
#define GL_TEXTURE                        0x1702

#define GL_FLAT                           0x1D00
#define GL_SMOOTH                         0x1D01

#define GL_EXP                            0x0800
#define GL_EXP2                           0x0801

#define GL_MODULATE                       0x2100
#define GL_DECAL                          0x2101
#define GL_ADD                            0x0104
#define GL_TEXTURE_ENV_MODE               0x2200
#define GL_TEXTURE_ENV                    0x2300

#define GL_S                              0x2000
#define GL_T                              0x2001
#define GL_R                              0x2002
#define GL_Q                              0x2003
#define GL_EYE_LINEAR                     0x2400
#define GL_OBJECT_LINEAR                  0x2401
#define GL_SPHERE_MAP                     0x2402
#define GL_TEXTURE_GEN_MODE               0x2500
#define GL_OBJECT_PLANE                   0x2501
#define GL_EYE_PLANE                      0x2502
#define GL_CLAMP                          0x2900

#define GL_LIGHT0                         0x4000
#define GL_LIGHT1                         0x4001
#define GL_LIGHT2                         0x4002
#define GL_LIGHT3                         0x4003
#define GL_LIGHT4                         0x4004
#define GL_LIGHT5                         0x4005
#define GL_LIGHT6                         0x4006
#define GL_LIGHT7                         0x4007

#define GL_VERTEX_ARRAY                   0x8074
#define GL_NORMAL_ARRAY                   0x8075
#define GL_COLOR_ARRAY                    0x8076
#define GL_TEXTURE_COORD_ARRAY            0x8078

// GL_EXT_compiled_vertex_array (never available; the game checks for null)
typedef void (*PFNGLLOCKARRAYSEXTPROC)(GLint first, GLsizei count);
typedef void (*PFNGLUNLOCKARRAYSEXTPROC)(void);

// ---------------------------------------------------------------------
// GLU subset
// ---------------------------------------------------------------------
#define GLU_SMOOTH                        100000
#define GLU_FLAT                          100001
#define GLU_NONE                          100002
#define GLU_POINT                         100010
#define GLU_LINE                          100011
#define GLU_FILL                          100012
#define GLU_SILHOUETTE                    100013
#define GLU_OUTSIDE                       100020
#define GLU_INSIDE                        100021

struct GLUquadric {
	GLenum normals = GLU_SMOOTH;
	GLenum orientation = GLU_OUTSIDE;
	GLenum drawStyle = GLU_FILL;
	bool texture = false;
};
typedef GLUquadric GLUquadricObj;

GLUquadric* gluNewQuadric();
void gluDeleteQuadric(GLUquadric* q);
void gluQuadricDrawStyle(GLUquadric* q, GLenum style);
void gluQuadricOrientation(GLUquadric* q, GLenum orientation);
void gluQuadricNormals(GLUquadric* q, GLenum normals);
void gluQuadricTexture(GLUquadric* q, GLboolean texture);
void gluSphere(GLUquadric* q, GLdouble radius, GLint slices, GLint stacks);
void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
               GLdouble centerX, GLdouble centerY, GLdouble centerZ,
               GLdouble upX, GLdouble upY, GLdouble upZ);
const GLubyte* gluErrorString(GLenum error);

// ---------------------------------------------------------------------
// Legacy entry points implemented by the compatibility layer
// ---------------------------------------------------------------------
// glGetError() forces a synchronous round trip to the GPU process in
// WebGL, and the game calls it every frame. Release builds report
// GL_NO_ERROR without querying; debug builds (GLC_CHECK_ERRORS) query.
GLenum glcGetError();
void glcEnable(GLenum cap);
void glcDisable(GLenum cap);
GLboolean glcIsEnabled(GLenum cap);
void glcHint(GLenum target, GLenum mode);
void glcGetIntegerv(GLenum pname, GLint* data);
void glcGetFloatv(GLenum pname, GLfloat* data);
void glcGetBooleanv(GLenum pname, GLboolean* data);
const GLubyte* glcGetString(GLenum name);
void glcBindTexture(GLenum target, GLuint texture);
void glcDeleteTextures(GLsizei n, const GLuint* textures);
void glcTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                   GLint border, GLenum format, GLenum type, const void* pixels);
void glcGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params);
void glcDrawArrays(GLenum mode, GLint first, GLsizei count);
void glcDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices);
void glcViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glcBlendFunc(GLenum sfactor, GLenum dfactor);
void glcDepthFunc(GLenum func);
void glcDepthMask(GLboolean flag);

void glcMatrixMode(GLenum mode);
void glcLoadIdentity();
void glcPushMatrix();
void glcPopMatrix();
void glcLoadMatrixf(const GLfloat* m);
void glcLoadMatrixd(const GLdouble* m);
void glcMultMatrixf(const GLfloat* m);
void glcMultMatrixd(const GLdouble* m);
void glcTranslatef(GLfloat x, GLfloat y, GLfloat z);
void glcTranslated(GLdouble x, GLdouble y, GLdouble z);
void glcRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void glcRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void glcScalef(GLfloat x, GLfloat y, GLfloat z);
void glcScaled(GLdouble x, GLdouble y, GLdouble z);
void glcOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);
void glcFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f);

void glcBegin(GLenum mode);
void glcEnd();
void glcVertex2f(GLfloat x, GLfloat y);
void glcVertex2i(GLint x, GLint y);
void glcVertex3f(GLfloat x, GLfloat y, GLfloat z);
void glcVertex3d(GLdouble x, GLdouble y, GLdouble z);
void glcVertex3fv(const GLfloat* v);
void glcVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void glcNormal3f(GLfloat x, GLfloat y, GLfloat z);
void glcNormal3d(GLdouble x, GLdouble y, GLdouble z);
void glcNormal3i(GLint x, GLint y, GLint z);
void glcNormal3fv(const GLfloat* v);
void glcTexCoord2f(GLfloat s, GLfloat t);
void glcTexCoord2d(GLdouble s, GLdouble t);
void glcTexCoord2i(GLint s, GLint t);
void glcColor3f(GLfloat r, GLfloat g, GLfloat b);
void glcColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void glcColor4fv(const GLfloat* v);
void glcColor3ub(GLubyte r, GLubyte g, GLubyte b);
void glcColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void glcColor4ubv(const GLubyte* v);

void glcEnableClientState(GLenum array);
void glcDisableClientState(GLenum array);
void glcVertexPointer(GLint size, GLenum type, GLsizei stride, const void* ptr);
void glcNormalPointer(GLenum type, GLsizei stride, const void* ptr);
void glcColorPointer(GLint size, GLenum type, GLsizei stride, const void* ptr);
void glcTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* ptr);

void glcShadeModel(GLenum mode);
void glcAlphaFunc(GLenum func, GLclampf ref);
void glcLightfv(GLenum light, GLenum pname, const GLfloat* params);
void glcLightf(GLenum light, GLenum pname, GLfloat param);
void glcLightModelfv(GLenum pname, const GLfloat* params);
void glcLightModeli(GLenum pname, GLint param);
void glcMaterialfv(GLenum face, GLenum pname, const GLfloat* params);
void glcMaterialf(GLenum face, GLenum pname, GLfloat param);
void glcColorMaterial(GLenum face, GLenum mode);
void glcFogf(GLenum pname, GLfloat param);
void glcFogi(GLenum pname, GLint param);
void glcFogfv(GLenum pname, const GLfloat* params);
void glcTexEnvf(GLenum target, GLenum pname, GLfloat param);
void glcTexEnvi(GLenum target, GLenum pname, GLint param);
void glcTexGeni(GLenum coord, GLenum pname, GLint param);
void glcTexGenfv(GLenum coord, GLenum pname, const GLfloat* params);

#ifndef GLC_NO_REDIRECT
#define glGetError               glcGetError
#define glEnable                 glcEnable
#define glDisable                glcDisable
#define glIsEnabled              glcIsEnabled
#define glHint                   glcHint
#define glGetIntegerv            glcGetIntegerv
#define glGetFloatv              glcGetFloatv
#define glGetBooleanv            glcGetBooleanv
#define glGetString              glcGetString
#define glBindTexture            glcBindTexture
#define glDeleteTextures         glcDeleteTextures
#define glTexImage2D             glcTexImage2D
#define glGetTexLevelParameteriv glcGetTexLevelParameteriv
#define glDrawArrays             glcDrawArrays
#define glDrawElements           glcDrawElements
#define glViewport               glcViewport
#define glBlendFunc              glcBlendFunc
#define glDepthFunc              glcDepthFunc
#define glDepthMask              glcDepthMask

#define glMatrixMode             glcMatrixMode
#define glLoadIdentity           glcLoadIdentity
#define glPushMatrix             glcPushMatrix
#define glPopMatrix              glcPopMatrix
#define glLoadMatrixf            glcLoadMatrixf
#define glLoadMatrixd            glcLoadMatrixd
#define glMultMatrixf            glcMultMatrixf
#define glMultMatrixd            glcMultMatrixd
#define glTranslatef             glcTranslatef
#define glTranslated             glcTranslated
#define glRotatef                glcRotatef
#define glRotated                glcRotated
#define glScalef                 glcScalef
#define glScaled                 glcScaled
#define glOrtho                  glcOrtho
#define glFrustum                glcFrustum

#define glBegin                  glcBegin
#define glEnd                    glcEnd
#define glVertex2f               glcVertex2f
#define glVertex2i               glcVertex2i
#define glVertex3f               glcVertex3f
#define glVertex3d               glcVertex3d
#define glVertex3fv              glcVertex3fv
#define glVertex4f               glcVertex4f
#define glNormal3f               glcNormal3f
#define glNormal3d               glcNormal3d
#define glNormal3i               glcNormal3i
#define glNormal3fv              glcNormal3fv
#define glTexCoord2f             glcTexCoord2f
#define glTexCoord2d             glcTexCoord2d
#define glTexCoord2i             glcTexCoord2i
#define glColor3f                glcColor3f
#define glColor4f                glcColor4f
#define glColor4fv               glcColor4fv
#define glColor3ub               glcColor3ub
#define glColor4ub               glcColor4ub
#define glColor4ubv              glcColor4ubv

#define glEnableClientState      glcEnableClientState
#define glDisableClientState     glcDisableClientState
#define glVertexPointer          glcVertexPointer
#define glNormalPointer          glcNormalPointer
#define glColorPointer           glcColorPointer
#define glTexCoordPointer        glcTexCoordPointer

#define glShadeModel             glcShadeModel
#define glAlphaFunc              glcAlphaFunc
#define glLightfv                glcLightfv
#define glLightf                 glcLightf
#define glLightModelfv           glcLightModelfv
#define glLightModeli            glcLightModeli
#define glMaterialfv             glcMaterialfv
#define glMaterialf              glcMaterialf
#define glColorMaterial          glcColorMaterial
#define glFogf                   glcFogf
#define glFogi                   glcFogi
#define glFogfv                  glcFogfv
#define glTexEnvf                glcTexEnvf
#define glTexEnvi                glcTexEnvi
#define glTexGeni                glcTexGeni
#define glTexGenfv               glcTexGenfv
#endif // GLC_NO_REDIRECT

// ---------------------------------------------------------------------
// Internal API used by the SFML compatibility layer
// ---------------------------------------------------------------------
namespace glc {

// Must be called once after the WebGL context has been made current.
bool Init();

// Snapshot / restore of the complete emulated state (used to emulate
// sf::RenderTarget::pushGLStates()/popGLStates()).
void PushAllState();
void PopAllState();

// Puts the pipeline into the state SFML's resetGLStates() establishes:
// no lighting, depth test, culling, alpha test, fog, texgen or stencil;
// alpha blending and texturing on.
void ResetForSFML();

struct Vertex2D {
	float x, y;
	unsigned char r, g, b, a;
	float u, v;          // normalized texture coordinates
};

enum BlendPreset { BlendAlpha = 0, BlendAdd, BlendMultiply, BlendNone };

// Draws 2D geometry with the given 4x4 column-major projection and
// model matrices. 'texture' may be 0. Assumes the caller has put the
// pipeline into SFML state (ResetForSFML) or saved it.
void Draw2D(const Vertex2D* verts, int count, GLenum primitive,
            const float* projection, const float* model,
            GLuint texture, BlendPreset blend);

// Current framebuffer size used to restore the viewport after
// rendering to an off-screen target.
void SetDefaultFramebufferSize(int w, int h);

} // namespace glc

#endif // ETR_GL_COMPAT_H
