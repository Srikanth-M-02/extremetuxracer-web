/* --------------------------------------------------------------------
Extreme Tux Racer - WebGL port

Fixed-function OpenGL emulation on top of OpenGL ES 3.0 / WebGL 2.
See gl_compat.h for an overview.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
---------------------------------------------------------------------*/

#define GLC_NO_REDIRECT
#include "gl_compat.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace {

// =====================================================================
// Small matrix helpers (column-major, like OpenGL)
// =====================================================================

struct Mat4 {
	float m[16];
	static Mat4 Identity() {
		Mat4 r;
		std::memset(r.m, 0, sizeof(r.m));
		r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
		return r;
	}
};

Mat4 Mul(const Mat4& a, const Mat4& b) {
	Mat4 r;
	for (int c = 0; c < 4; c++) {
		for (int row = 0; row < 4; row++) {
			r.m[c * 4 + row] =
			    a.m[0 * 4 + row] * b.m[c * 4 + 0] +
			    a.m[1 * 4 + row] * b.m[c * 4 + 1] +
			    a.m[2 * 4 + row] * b.m[c * 4 + 2] +
			    a.m[3 * 4 + row] * b.m[c * 4 + 3];
		}
	}
	return r;
}

void TransformVec4(const Mat4& m, const float in[4], float out[4]) {
	for (int row = 0; row < 4; row++)
		out[row] = m.m[row] * in[0] + m.m[4 + row] * in[1] + m.m[8 + row] * in[2] + m.m[12 + row] * in[3];
}

// Inverse-transpose of the upper-left 3x3 block (the normal matrix).
void NormalMatrix(const Mat4& m, float out[9]) {
	const float a = m.m[0], b = m.m[4], c = m.m[8];
	const float d = m.m[1], e = m.m[5], f = m.m[9];
	const float g = m.m[2], h = m.m[6], i = m.m[10];
	const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
	const float D = -(b * i - c * h), E = a * i - c * g, F = -(a * h - b * g);
	const float G = b * f - c * e, H = -(a * f - c * d), I = a * e - b * d;
	float det = a * A + b * B + c * C;
	if (std::fabs(det) < 1e-20f) det = 1e-20f;
	const float inv = 1.f / det;
	// inverse = adj^T / det; (inverse)^T = adj / det, stored column-major
	out[0] = A * inv; out[3] = D * inv; out[6] = G * inv;
	out[1] = B * inv; out[4] = E * inv; out[7] = H * inv;
	out[2] = C * inv; out[5] = F * inv; out[8] = I * inv;
}

// =====================================================================
// Emulated state
// =====================================================================

constexpr int kMaxLights = 8;
constexpr int kMaxStackDepth = 32;

struct LightState {
	bool enabled = false;
	float ambient[4] = {0, 0, 0, 1};
	float diffuse[4] = {0, 0, 0, 1};
	float specular[4] = {0, 0, 0, 1};
	float position[4] = {0, 0, 1, 0};   // eye space
	float attenuation[3] = {1, 0, 0};
};

struct ClientArray {
	bool enabled = false;
	GLint size = 4;
	GLenum type = GL_FLOAT;
	GLsizei stride = 0;
	const void* ptr = nullptr;
};

enum ArrayIdx { A_VERTEX = 0, A_COLOR, A_NORMAL, A_TEXCOORD, A_COUNT };

struct State {
	// emulated capabilities
	bool texture2D = false;
	bool lighting = false;
	bool normalize = false;
	bool alphaTest = false;
	bool colorMaterial = false;
	bool fog = false;
	bool texGenS = false;
	bool texGenT = false;
	// real capabilities (tracked so they can be saved and restored)
	bool blend = false;
	bool depthTest = false;
	bool cullFace = false;
	bool stencilTest = false;
	bool scissorTest = false;
	GLenum blendSrc = GL_ONE, blendDst = GL_ZERO;
	GLenum depthFunc = GL_LESS;
	bool depthMask = true;

	GLenum matrixMode = GL_MODELVIEW;

	float color[4] = {1, 1, 1, 1};
	float normal[3] = {0, 0, 1};
	float texcoord[2] = {0, 0};

	LightState lights[kMaxLights];
	float lightModelAmbient[4] = {0.2f, 0.2f, 0.2f, 1.f};

	float matAmbient[4] = {0.2f, 0.2f, 0.2f, 1.f};
	float matDiffuse[4] = {0.8f, 0.8f, 0.8f, 1.f};
	float matSpecular[4] = {0, 0, 0, 1};
	float matEmission[4] = {0, 0, 0, 1};
	float matShininess = 0.f;

	GLenum fogMode = GL_EXP;
	float fogStart = 0.f, fogEnd = 1.f, fogDensity = 1.f;
	float fogColor[4] = {0, 0, 0, 0};

	GLenum texEnvMode = GL_MODULATE;
	float planeS[4] = {1, 0, 0, 0};
	float planeT[4] = {0, 1, 0, 0};

	GLenum alphaFunc = GL_ALWAYS;
	float alphaRef = 0.f;

	GLuint boundTexture = 0;
	GLint viewport[4] = {0, 0, 0, 0};

	ClientArray arrays[A_COUNT];
};

State S;
std::vector<Mat4> modelviewStack(1, Mat4::Identity());
std::vector<Mat4> projectionStack(1, Mat4::Identity());
std::vector<Mat4> textureStack(1, Mat4::Identity());
unsigned modelviewVersion = 1;

struct SavedState {
	State state;
	Mat4 modelview, projection, texture;
};
std::vector<SavedState> savedStates;

struct TexInfo { int w, h; };
std::unordered_map<GLuint, TexInfo> textureSizes;

int defaultFbWidth = 0, defaultFbHeight = 0;

std::vector<Mat4>& CurrentStack() {
	switch (S.matrixMode) {
		case GL_PROJECTION: return projectionStack;
		case GL_TEXTURE: return textureStack;
		default: return modelviewStack;
	}
}

void MatrixChanged() {
	if (S.matrixMode == GL_MODELVIEW) modelviewVersion++;
}

Mat4& CurrentMatrix() { return CurrentStack().back(); }

// =====================================================================
// Shader program
// =====================================================================

const char* kVertexShader = R"GLSL(#version 300 es
precision highp float;
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec3 aNormal;
layout(location = 3) in vec4 aTex;

uniform mat4 uModelview;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

uniform bool uLighting;
uniform bool uNormalize;
uniform bool uColorMaterial;
uniform int uLightMask;
uniform vec4 uLightAmbient[8];
uniform vec4 uLightDiffuse[8];
uniform vec4 uLightSpecular[8];
uniform vec4 uLightPosition[8];
uniform vec3 uLightAtten[8];
uniform vec4 uGlobalAmbient;
uniform vec4 uMatAmbient;
uniform vec4 uMatDiffuse;
uniform vec4 uMatSpecular;
uniform vec4 uMatEmission;
uniform float uMatShininess;

uniform bool uTexGenS;
uniform bool uTexGenT;
uniform vec4 uPlaneS;
uniform vec4 uPlaneT;

out vec4 vColor;
out vec2 vTex;
out float vFogCoord;

void main() {
	vec4 eye = uModelview * aPos;
	gl_Position = uProjection * eye;

	vec4 col = aColor;
	if (uLighting) {
		vec3 n = uNormalMatrix * aNormal;
		if (uNormalize) n = normalize(n);
		vec4 amb = uColorMaterial ? aColor : uMatAmbient;
		vec4 dif = uColorMaterial ? aColor : uMatDiffuse;
		vec3 c = uMatEmission.rgb + amb.rgb * uGlobalAmbient.rgb;
		for (int i = 0; i < 8; i++) {
			if ((uLightMask & (1 << i)) == 0) continue;
			vec3 L;
			float att = 1.0;
			if (uLightPosition[i].w == 0.0) {
				L = normalize(uLightPosition[i].xyz);
			} else {
				vec3 d = uLightPosition[i].xyz / uLightPosition[i].w - eye.xyz;
				float dist = length(d);
				L = d / max(dist, 1e-6);
				att = 1.0 / max(uLightAtten[i].x + uLightAtten[i].y * dist + uLightAtten[i].z * dist * dist, 1e-6);
			}
			float ndl = max(dot(n, L), 0.0);
			vec3 term = amb.rgb * uLightAmbient[i].rgb + ndl * dif.rgb * uLightDiffuse[i].rgb;
			if (ndl > 0.0) {
				vec3 h = normalize(L + vec3(0.0, 0.0, 1.0));
				float nh = max(dot(n, h), 0.0);
				float s = (uMatShininess == 0.0) ? 1.0 : pow(nh, uMatShininess);
				term += s * uMatSpecular.rgb * uLightSpecular[i].rgb;
			}
			c += att * term;
		}
		col = vec4(clamp(c, 0.0, 1.0), dif.a);
	}
	vColor = clamp(col, 0.0, 1.0);

	vec2 t = aTex.xy;
	if (uTexGenS) t.x = dot(uPlaneS, aPos);
	if (uTexGenT) t.y = dot(uPlaneT, aPos);
	vTex = t;
	vFogCoord = abs(eye.z);
}
)GLSL";

const char* kFragmentShader = R"GLSL(#version 300 es
precision highp float;
in vec4 vColor;
in vec2 vTex;
in float vFogCoord;

uniform bool uTexture;
uniform int uTexEnv;        // 0 modulate, 1 decal, 2 replace, 3 add
uniform sampler2D uSampler;
uniform int uAlphaFunc;     // 0 always, 1 never, 2 less, 3 lequal, 4 greater, 5 gequal, 6 equal, 7 notequal
uniform float uAlphaRef;
uniform bool uFog;
uniform int uFogMode;       // 0 linear, 1 exp, 2 exp2
uniform float uFogStart;
uniform float uFogEnd;
uniform float uFogDensity;
uniform vec4 uFogColor;

out vec4 fragColor;

void main() {
	vec4 c = vColor;
	if (uTexture) {
		vec4 t = texture(uSampler, vTex);
		if (uTexEnv == 0) c = c * t;
		else if (uTexEnv == 1) c = vec4(mix(c.rgb, t.rgb, t.a), c.a);
		else if (uTexEnv == 2) c = t;
		else c = vec4(min(c.rgb + t.rgb, vec3(1.0)), c.a * t.a);
	}
	if (uAlphaFunc != 0) {
		float a = c.a;
		bool pass = true;
		if (uAlphaFunc == 1) pass = false;
		else if (uAlphaFunc == 2) pass = a < uAlphaRef;
		else if (uAlphaFunc == 3) pass = a <= uAlphaRef;
		else if (uAlphaFunc == 4) pass = a > uAlphaRef;
		else if (uAlphaFunc == 5) pass = a >= uAlphaRef;
		else if (uAlphaFunc == 6) pass = a == uAlphaRef;
		else if (uAlphaFunc == 7) pass = a != uAlphaRef;
		if (!pass) discard;
	}
	if (uFog) {
		float f;
		if (uFogMode == 0) f = (uFogEnd - vFogCoord) / max(uFogEnd - uFogStart, 1e-6);
		else if (uFogMode == 1) f = exp(-uFogDensity * vFogCoord);
		else { float e = uFogDensity * vFogCoord; f = exp(-e * e); }
		f = clamp(f, 0.0, 1.0);
		c.rgb = mix(uFogColor.rgb, c.rgb, f);
	}
	fragColor = c;
}
)GLSL";

GLuint program = 0;
GLuint vao = 0;
GLuint vertexBuffer = 0;
GLuint indexBuffer = 0;

struct Uniforms {
	GLint modelview, projection, normalMatrix;
	GLint lighting, normalize, colorMaterial, lightMask;
	GLint lightAmbient, lightDiffuse, lightSpecular, lightPosition, lightAtten;
	GLint globalAmbient, matAmbient, matDiffuse, matSpecular, matEmission, matShininess;
	GLint texGenS, texGenT, planeS, planeT;
	GLint texture, texEnv, sampler, alphaFunc, alphaRef;
	GLint fog, fogMode, fogStart, fogEnd, fogDensity, fogColor;
} U;

// Cache of last uploaded uniform values, keyed by location.
struct UniformCache {
	std::unordered_map<GLint, std::vector<float>> values;
	bool Changed(GLint loc, const float* v, std::size_t n) {
		std::vector<float>& cached = values[loc];
		if (cached.size() == n && std::memcmp(cached.data(), v, n * sizeof(float)) == 0)
			return false;
		cached.assign(v, v + n);
		return true;
	}
	void Mat4(GLint loc, const float* v) { if (loc >= 0 && Changed(loc, v, 16)) glUniformMatrix4fv(loc, 1, GL_FALSE, v); }
	void Mat3(GLint loc, const float* v) { if (loc >= 0 && Changed(loc, v, 9)) glUniformMatrix3fv(loc, 1, GL_FALSE, v); }
	void Vec4(GLint loc, const float* v, int count = 1) { if (loc >= 0 && Changed(loc, v, 4 * count)) glUniform4fv(loc, count, v); }
	void Vec3(GLint loc, const float* v, int count = 1) { if (loc >= 0 && Changed(loc, v, 3 * count)) glUniform3fv(loc, count, v); }
	void Float(GLint loc, float v) { if (loc >= 0 && Changed(loc, &v, 1)) glUniform1f(loc, v); }
	void Int(GLint loc, int i) { float v = (float)i; if (loc >= 0 && Changed(loc, &v, 1)) glUniform1i(loc, i); }
} UC;

GLuint CompileShader(GLenum type, const char* src) {
	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, &src, nullptr);
	glCompileShader(sh);
	GLint ok = 0;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char log[2048];
		glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
		std::fprintf(stderr, "glcompat: shader compile error: %s\n", log);
	}
	return sh;
}

// =====================================================================
// Draw helpers
// =====================================================================

int AlphaFuncIndex(GLenum f) {
	switch (f) {
		case GL_NEVER: return 1;
		case GL_LESS: return 2;
		case GL_LEQUAL: return 3;
		case GL_GREATER: return 4;
		case GL_GEQUAL: return 5;
		case GL_EQUAL: return 6;
		case GL_NOTEQUAL: return 7;
		default: return 0;
	}
}

void ApplyUniforms() {
	glUseProgram(program);

	const Mat4& mv = modelviewStack.back();
	UC.Mat4(U.modelview, mv.m);
	UC.Mat4(U.projection, projectionStack.back().m);

	UC.Int(U.lighting, S.lighting ? 1 : 0);
	if (S.lighting) {
		static unsigned normalVersion = 0;
		static float normalMat[9];
		if (normalVersion != modelviewVersion) {
			NormalMatrix(mv, normalMat);
			normalVersion = modelviewVersion;
		}
		UC.Mat3(U.normalMatrix, normalMat);
		UC.Int(U.normalize, S.normalize ? 1 : 0);
		UC.Int(U.colorMaterial, S.colorMaterial ? 1 : 0);
		int mask = 0;
		float amb[kMaxLights * 4], dif[kMaxLights * 4], spec[kMaxLights * 4], pos[kMaxLights * 4], att[kMaxLights * 3];
		for (int i = 0; i < kMaxLights; i++) {
			const LightState& l = S.lights[i];
			if (l.enabled) mask |= 1 << i;
			std::memcpy(amb + i * 4, l.ambient, 16);
			std::memcpy(dif + i * 4, l.diffuse, 16);
			std::memcpy(spec + i * 4, l.specular, 16);
			std::memcpy(pos + i * 4, l.position, 16);
			std::memcpy(att + i * 3, l.attenuation, 12);
		}
		UC.Int(U.lightMask, mask);
		UC.Vec4(U.lightAmbient, amb, kMaxLights);
		UC.Vec4(U.lightDiffuse, dif, kMaxLights);
		UC.Vec4(U.lightSpecular, spec, kMaxLights);
		UC.Vec4(U.lightPosition, pos, kMaxLights);
		UC.Vec3(U.lightAtten, att, kMaxLights);
		UC.Vec4(U.globalAmbient, S.lightModelAmbient);
		UC.Vec4(U.matAmbient, S.matAmbient);
		UC.Vec4(U.matDiffuse, S.matDiffuse);
		UC.Vec4(U.matSpecular, S.matSpecular);
		UC.Vec4(U.matEmission, S.matEmission);
		UC.Float(U.matShininess, S.matShininess);
	}

	UC.Int(U.texGenS, S.texGenS ? 1 : 0);
	UC.Int(U.texGenT, S.texGenT ? 1 : 0);
	if (S.texGenS) UC.Vec4(U.planeS, S.planeS);
	if (S.texGenT) UC.Vec4(U.planeT, S.planeT);

	const bool tex = S.texture2D && S.boundTexture != 0;
	UC.Int(U.texture, tex ? 1 : 0);
	if (tex) {
		int env = 0;
		switch (S.texEnvMode) {
			case GL_DECAL: env = 1; break;
			case GL_REPLACE: env = 2; break;
			case GL_ADD: env = 3; break;
			default: env = 0; break;
		}
		UC.Int(U.texEnv, env);
	}

	UC.Int(U.alphaFunc, S.alphaTest ? AlphaFuncIndex(S.alphaFunc) : 0);
	if (S.alphaTest) UC.Float(U.alphaRef, S.alphaRef);

	UC.Int(U.fog, S.fog ? 1 : 0);
	if (S.fog) {
		UC.Int(U.fogMode, S.fogMode == GL_LINEAR ? 0 : (S.fogMode == GL_EXP ? 1 : 2));
		UC.Float(U.fogStart, S.fogStart);
		UC.Float(U.fogEnd, S.fogEnd);
		UC.Float(U.fogDensity, S.fogDensity);
		UC.Vec4(U.fogColor, S.fogColor);
	}
}

void SetConstantAttribs(bool vertex, bool color, bool normal, bool tex) {
	if (!vertex) { glDisableVertexAttribArray(0); glVertexAttrib4f(0, 0, 0, 0, 1); }
	if (!color) { glDisableVertexAttribArray(1); glVertexAttrib4fv(1, S.color); }
	if (!normal) { glDisableVertexAttribArray(2); glVertexAttrib4f(2, S.normal[0], S.normal[1], S.normal[2], 0); }
	if (!tex) { glDisableVertexAttribArray(3); glVertexAttrib4f(3, S.texcoord[0], S.texcoord[1], 0, 1); }
}

// Converts legacy primitive types into ones GLES understands. If indices
// are required they are appended to 'out' (relative to the vertex range
// starting at 0) and true is returned.
std::vector<uint32_t> primIndices;

GLenum NativeMode(GLenum mode) {
	switch (mode) {
		case GL_QUADS: return GL_TRIANGLES;
		case GL_QUAD_STRIP: return GL_TRIANGLE_STRIP;
		case GL_POLYGON: return GL_TRIANGLE_FAN;
		default: return mode;
	}
}

// Expands a list of quad vertex indices into triangle indices.
void QuadsToTriangles(const uint32_t* idx, std::size_t count, std::vector<uint32_t>& out) {
	out.clear();
	out.reserve(count / 4 * 6);
	for (std::size_t q = 0; q + 3 < count; q += 4) {
		out.push_back(idx[q]); out.push_back(idx[q + 1]); out.push_back(idx[q + 2]);
		out.push_back(idx[q]); out.push_back(idx[q + 2]); out.push_back(idx[q + 3]);
	}
}

void DrawIndexed(GLenum mode, const uint32_t* indices, std::size_t count, uint32_t maxIndex) {
	if (count == 0) return;
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	if (maxIndex < 65535) {
		static std::vector<uint16_t> shorts;
		shorts.resize(count);
		for (std::size_t i = 0; i < count; i++) shorts[i] = (uint16_t)indices[i];
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint16_t), shorts.data(), GL_STREAM_DRAW);
		glDrawElements(mode, (GLsizei)count, GL_UNSIGNED_SHORT, nullptr);
	} else {
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STREAM_DRAW);
		glDrawElements(mode, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
	}
}

// Issues a draw of 'count' sequential vertices (already uploaded at 0).
void DrawSequential(GLenum mode, std::size_t count) {
	if (count == 0) return;
	if (mode == GL_QUADS) {
		static std::vector<uint32_t> seq;
		seq.resize(count);
		for (std::size_t i = 0; i < count; i++) seq[i] = (uint32_t)i;
		QuadsToTriangles(seq.data(), count, primIndices);
		DrawIndexed(GL_TRIANGLES, primIndices.data(), primIndices.size(), (uint32_t)count);
	} else {
		GLsizei n = (GLsizei)count;
		if (mode == GL_QUAD_STRIP) n &= ~1;
		glDrawArrays(NativeMode(mode), 0, n);
	}
}

std::size_t TypeSize(GLenum type) {
	switch (type) {
		case GL_BYTE: case GL_UNSIGNED_BYTE: return 1;
		case GL_SHORT: case GL_UNSIGNED_SHORT: return 2;
		case GL_INT: case GL_UNSIGNED_INT: case GL_FLOAT: return 4;
		case GL_DOUBLE: return 8;
		default: return 4;
	}
}

bool IsNativeType(GLenum type) {
	return type == GL_FLOAT || type == GL_SHORT || type == GL_UNSIGNED_SHORT ||
	       type == GL_BYTE || type == GL_UNSIGNED_BYTE;
}

float ReadComponent(const unsigned char* p, GLenum type, int i) {
	switch (type) {
		case GL_BYTE: return ((const int8_t*)p)[i];
		case GL_UNSIGNED_BYTE: return ((const uint8_t*)p)[i];
		case GL_SHORT: { int16_t v; std::memcpy(&v, p + i * 2, 2); return v; }
		case GL_UNSIGNED_SHORT: { uint16_t v; std::memcpy(&v, p + i * 2, 2); return v; }
		case GL_INT: { int32_t v; std::memcpy(&v, p + i * 4, 4); return (float)v; }
		case GL_UNSIGNED_INT: { uint32_t v; std::memcpy(&v, p + i * 4, 4); return (float)v; }
		case GL_DOUBLE: { double v; std::memcpy(&v, p + i * 8, 8); return (float)v; }
		default: { float v; std::memcpy(&v, p + i * 4, 4); return v; }
	}
}

int ArrayComponents(int a) {
	return a == A_NORMAL ? 3 : S.arrays[a].size;
}

bool AttribNormalized(int a, GLenum type) {
	if (type == GL_FLOAT) return false;
	return a == A_COLOR || a == A_NORMAL;
}

// Uploads client-side arrays for vertices [lo, hi] and configures the
// vertex attributes so that vertex 'lo' is at index 0.
void UploadClientArrays(uint32_t lo, uint32_t hi) {
	const std::size_t n = (std::size_t)hi - lo + 1;
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);

	struct Info { int a; std::size_t elem; std::size_t stride; const unsigned char* ptr; GLenum type; };
	Info infos[A_COUNT];
	int numInfos = 0;
	bool allNative = true;
	for (int a = 0; a < A_COUNT; a++) {
		const ClientArray& ca = S.arrays[a];
		if (!ca.enabled || ca.ptr == nullptr) continue;
		Info inf;
		inf.a = a;
		inf.type = ca.type;
		inf.elem = ArrayComponents(a) * TypeSize(ca.type);
		inf.stride = ca.stride ? (std::size_t)ca.stride : inf.elem;
		inf.ptr = (const unsigned char*)ca.ptr;
		if (!IsNativeType(ca.type)) allNative = false;
		infos[numInfos++] = inf;
	}

	bool enabled[A_COUNT] = {false, false, false, false};

	// Fast path: all arrays interleaved in one block with a common stride
	// (this is how the terrain is stored). Upload the bytes as-is.
	bool interleaved = allNative && numInfos > 0;
	const unsigned char* base = nullptr;
	if (interleaved) {
		base = infos[0].ptr;
		for (int i = 1; i < numInfos; i++) if (infos[i].ptr < base) base = infos[i].ptr;
		for (int i = 0; i < numInfos; i++) {
			if (infos[i].stride != infos[0].stride ||
			    (std::size_t)(infos[i].ptr - base) + infos[i].elem > infos[i].stride) {
				interleaved = false;
				break;
			}
		}
	}

	if (interleaved && numInfos > 1) {
		const std::size_t stride = infos[0].stride;
		std::size_t maxEnd = 0;
		for (int i = 0; i < numInfos; i++)
			maxEnd = std::max(maxEnd, (std::size_t)(infos[i].ptr - base) + infos[i].elem);
		const std::size_t bytes = (n - 1) * stride + maxEnd;
		glBufferData(GL_ARRAY_BUFFER, bytes, base + lo * stride, GL_STREAM_DRAW);
		for (int i = 0; i < numInfos; i++) {
			const Info& inf = infos[i];
			glEnableVertexAttribArray(inf.a);
			glVertexAttribPointer(inf.a, ArrayComponents(inf.a), inf.type, AttribNormalized(inf.a, inf.type),
			                      (GLsizei)stride, (const void*)(uintptr_t)(inf.ptr - base));
			enabled[inf.a] = true;
		}
	} else {
		// General path: pack each array tightly (converting unsupported
		// component types to float) into one staging buffer.
		static std::vector<unsigned char> staging;
		std::size_t offsets[A_COUNT];
		GLenum types[A_COUNT];
		std::size_t total = 0;
		for (int i = 0; i < numInfos; i++) {
			const Info& inf = infos[i];
			offsets[i] = total;
			types[i] = IsNativeType(inf.type) ? inf.type : GL_FLOAT;
			const std::size_t elem = ArrayComponents(inf.a) * TypeSize(types[i]);
			total += (elem * n + 3) & ~(std::size_t)3;
		}
		staging.resize(total);
		for (int i = 0; i < numInfos; i++) {
			const Info& inf = infos[i];
			const int comps = ArrayComponents(inf.a);
			unsigned char* dst = staging.data() + offsets[i];
			if (types[i] == inf.type) {
				const std::size_t elem = inf.elem;
				const unsigned char* src = inf.ptr + lo * inf.stride;
				if (inf.stride == elem) {
					std::memcpy(dst, src, elem * n);
				} else {
					for (std::size_t v = 0; v < n; v++)
						std::memcpy(dst + v * elem, src + v * inf.stride, elem);
				}
			} else {
				float* fdst = (float*)dst;
				for (std::size_t v = 0; v < n; v++) {
					const unsigned char* src = inf.ptr + (lo + v) * inf.stride;
					for (int c = 0; c < comps; c++) fdst[v * comps + c] = ReadComponent(src, inf.type, c);
				}
			}
		}
		glBufferData(GL_ARRAY_BUFFER, total, staging.data(), GL_STREAM_DRAW);
		for (int i = 0; i < numInfos; i++) {
			const Info& inf = infos[i];
			glEnableVertexAttribArray(inf.a);
			glVertexAttribPointer(inf.a, ArrayComponents(inf.a), types[i], AttribNormalized(inf.a, types[i]),
			                      0, (const void*)(uintptr_t)offsets[i]);
			enabled[inf.a] = true;
		}
	}

	SetConstantAttribs(enabled[A_VERTEX], enabled[A_COLOR], enabled[A_NORMAL], enabled[A_TEXCOORD]);
}

// --------------------------------------------------------------------
// Immediate mode
// --------------------------------------------------------------------

struct ImmVertex {
	float pos[4];
	float color[4];
	float normal[3];
	float tex[2];
};

bool inBegin = false;
GLenum immMode = GL_TRIANGLES;
std::vector<ImmVertex> immVerts;

void EmitVertex(float x, float y, float z, float w) {
	ImmVertex v;
	v.pos[0] = x; v.pos[1] = y; v.pos[2] = z; v.pos[3] = w;
	std::memcpy(v.color, S.color, sizeof(v.color));
	std::memcpy(v.normal, S.normal, sizeof(v.normal));
	std::memcpy(v.tex, S.texcoord, sizeof(v.tex));
	immVerts.push_back(v);
}

void FlushImmediate(GLenum mode) {
	if (immVerts.empty()) return;
	ApplyUniforms();
	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, immVerts.size() * sizeof(ImmVertex), immVerts.data(), GL_STREAM_DRAW);
	const GLsizei stride = sizeof(ImmVertex);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(ImmVertex, pos));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(ImmVertex, color));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(ImmVertex, normal));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(ImmVertex, tex));
	DrawSequential(mode, immVerts.size());
	immVerts.clear();
}

void ApplyRealCap(GLenum cap, bool on) {
	if (on) ::glEnable(cap);
	else ::glDisable(cap);
}

void SetCap(GLenum cap, bool on) {
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + kMaxLights) {
		S.lights[cap - GL_LIGHT0].enabled = on;
		return;
	}
	switch (cap) {
		case GL_TEXTURE_2D: S.texture2D = on; break;
		case GL_LIGHTING: S.lighting = on; break;
		case GL_NORMALIZE: S.normalize = on; break;
		case GL_ALPHA_TEST: S.alphaTest = on; break;
		case GL_COLOR_MATERIAL: S.colorMaterial = on; break;
		case GL_FOG: S.fog = on; break;
		case GL_TEXTURE_GEN_S: S.texGenS = on; break;
		case GL_TEXTURE_GEN_T: S.texGenT = on; break;
		case GL_BLEND: S.blend = on; ApplyRealCap(cap, on); break;
		case GL_DEPTH_TEST: S.depthTest = on; ApplyRealCap(cap, on); break;
		case GL_CULL_FACE: S.cullFace = on; ApplyRealCap(cap, on); break;
		case GL_STENCIL_TEST: S.stencilTest = on; ApplyRealCap(cap, on); break;
		case GL_SCISSOR_TEST: S.scissorTest = on; ApplyRealCap(cap, on); break;
		case GL_DITHER:
		case GL_POLYGON_OFFSET_FILL:
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		case GL_SAMPLE_COVERAGE:
			ApplyRealCap(cap, on);
			break;
		default:
			// Legacy capability without an equivalent (line smoothing,
			// texgen R/Q, ...): ignore instead of raising GL errors.
			break;
	}
}

void ApplyRealState() {
	ApplyRealCap(GL_BLEND, S.blend);
	ApplyRealCap(GL_DEPTH_TEST, S.depthTest);
	ApplyRealCap(GL_CULL_FACE, S.cullFace);
	ApplyRealCap(GL_STENCIL_TEST, S.stencilTest);
	ApplyRealCap(GL_SCISSOR_TEST, S.scissorTest);
	::glBlendFunc(S.blendSrc, S.blendDst);
	::glDepthFunc(S.depthFunc);
	::glDepthMask(S.depthMask ? GL_TRUE : GL_FALSE);
	::glBindTexture(GL_TEXTURE_2D, S.boundTexture);
	if (S.viewport[2] > 0 && S.viewport[3] > 0)
		::glViewport(S.viewport[0], S.viewport[1], S.viewport[2], S.viewport[3]);
}

void LoadMatrix(const Mat4& m) {
	CurrentMatrix() = m;
	MatrixChanged();
}

void MultMatrix(const Mat4& m) {
	CurrentMatrix() = Mul(CurrentMatrix(), m);
	MatrixChanged();
}

// Sphere vertex helper for gluSphere (triangles emitted in GL winding order).
struct SphereVert { float n[3]; float p[3]; float t[2]; };

void EmitSphereVert(const SphereVert& v, bool normals, bool texture) {
	if (normals) glcNormal3f(v.n[0], v.n[1], v.n[2]);
	if (texture) glcTexCoord2f(v.t[0], v.t[1]);
	glcVertex3f(v.p[0], v.p[1], v.p[2]);
}

} // namespace

// =====================================================================
// Internal API
// =====================================================================

namespace glc {

bool Init() {
	GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
	GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
	program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	GLint ok = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &ok);
	if (!ok) {
		char log[2048];
		glGetProgramInfoLog(program, sizeof(log), nullptr, log);
		std::fprintf(stderr, "glcompat: program link error: %s\n", log);
		return false;
	}
	glDeleteShader(vs);
	glDeleteShader(fs);

	auto loc = [](const char* name) { return glGetUniformLocation(program, name); };
	U.modelview = loc("uModelview");
	U.projection = loc("uProjection");
	U.normalMatrix = loc("uNormalMatrix");
	U.lighting = loc("uLighting");
	U.normalize = loc("uNormalize");
	U.colorMaterial = loc("uColorMaterial");
	U.lightMask = loc("uLightMask");
	U.lightAmbient = loc("uLightAmbient");
	U.lightDiffuse = loc("uLightDiffuse");
	U.lightSpecular = loc("uLightSpecular");
	U.lightPosition = loc("uLightPosition");
	U.lightAtten = loc("uLightAtten");
	U.globalAmbient = loc("uGlobalAmbient");
	U.matAmbient = loc("uMatAmbient");
	U.matDiffuse = loc("uMatDiffuse");
	U.matSpecular = loc("uMatSpecular");
	U.matEmission = loc("uMatEmission");
	U.matShininess = loc("uMatShininess");
	U.texGenS = loc("uTexGenS");
	U.texGenT = loc("uTexGenT");
	U.planeS = loc("uPlaneS");
	U.planeT = loc("uPlaneT");
	U.texture = loc("uTexture");
	U.texEnv = loc("uTexEnv");
	U.sampler = loc("uSampler");
	U.alphaFunc = loc("uAlphaFunc");
	U.alphaRef = loc("uAlphaRef");
	U.fog = loc("uFog");
	U.fogMode = loc("uFogMode");
	U.fogStart = loc("uFogStart");
	U.fogEnd = loc("uFogEnd");
	U.fogDensity = loc("uFogDensity");
	U.fogColor = loc("uFogColor");

	glUseProgram(program);
	glUniform1i(U.sampler, 0);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vertexBuffer);
	glGenBuffers(1, &indexBuffer);
	glActiveTexture(GL_TEXTURE0);

	// GL 1.x defaults for light 0
	for (int i = 0; i < 4; i++) {
		S.lights[0].diffuse[i] = 1.f;
		S.lights[0].specular[i] = 1.f;
	}
	ApplyRealState();
	return true;
}

void PushAllState() {
	SavedState s;
	s.state = S;
	s.modelview = modelviewStack.back();
	s.projection = projectionStack.back();
	s.texture = textureStack.back();
	savedStates.push_back(s);
}

void PopAllState() {
	if (savedStates.empty()) return;
	const SavedState& s = savedStates.back();
	S = s.state;
	modelviewStack.back() = s.modelview;
	projectionStack.back() = s.projection;
	textureStack.back() = s.texture;
	modelviewVersion++;
	savedStates.pop_back();
	ApplyRealState();
}

void ResetForSFML() {
	SetCap(GL_LIGHTING, false);
	SetCap(GL_DEPTH_TEST, false);
	SetCap(GL_CULL_FACE, false);
	SetCap(GL_ALPHA_TEST, false);
	SetCap(GL_FOG, false);
	SetCap(GL_TEXTURE_GEN_S, false);
	SetCap(GL_TEXTURE_GEN_T, false);
	SetCap(GL_STENCIL_TEST, false);
	SetCap(GL_TEXTURE_2D, true);
	SetCap(GL_BLEND, true);
	S.texEnvMode = GL_MODULATE;
	S.matrixMode = GL_MODELVIEW;
}

void SetDefaultFramebufferSize(int w, int h) {
	defaultFbWidth = w;
	defaultFbHeight = h;
}

void Draw2D(const Vertex2D* verts, int count, GLenum primitive,
            const float* projection, const float* model,
            GLuint texture, BlendPreset blend) {
	if (count <= 0) return;
	std::memcpy(projectionStack.back().m, projection, sizeof(float) * 16);
	std::memcpy(modelviewStack.back().m, model, sizeof(float) * 16);
	modelviewVersion++;

	switch (blend) {
		case BlendAdd: glcBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
		case BlendMultiply: glcBlendFunc(GL_DST_COLOR, GL_ZERO); break;
		case BlendNone: glcBlendFunc(GL_ONE, GL_ZERO); break;
		default: glcBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
	}
	glcBindTexture(GL_TEXTURE_2D, texture);

	const bool savedLighting = S.lighting;
	S.lighting = false;
	ApplyUniforms();
	S.lighting = savedLighting;

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex2D), verts, GL_STREAM_DRAW);
	const GLsizei stride = sizeof(Vertex2D);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex2D, x));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void*)offsetof(Vertex2D, r));
	glDisableVertexAttribArray(2);
	glVertexAttrib4f(2, 0, 0, 1, 0);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex2D, u));
	DrawSequential(primitive, (std::size_t)count);
}

} // namespace glc

// =====================================================================
// Legacy API implementation
// =====================================================================

GLenum glcGetError() {
#ifdef GLC_CHECK_ERRORS
	return ::glGetError();
#else
	return GL_NO_ERROR;
#endif
}

void glcEnable(GLenum cap) { SetCap(cap, true); }
void glcDisable(GLenum cap) { SetCap(cap, false); }

GLboolean glcIsEnabled(GLenum cap) {
	if (cap >= GL_LIGHT0 && cap < GL_LIGHT0 + kMaxLights) return S.lights[cap - GL_LIGHT0].enabled;
	switch (cap) {
		case GL_TEXTURE_2D: return S.texture2D;
		case GL_LIGHTING: return S.lighting;
		case GL_NORMALIZE: return S.normalize;
		case GL_ALPHA_TEST: return S.alphaTest;
		case GL_COLOR_MATERIAL: return S.colorMaterial;
		case GL_FOG: return S.fog;
		case GL_TEXTURE_GEN_S: return S.texGenS;
		case GL_TEXTURE_GEN_T: return S.texGenT;
		case GL_BLEND: return S.blend;
		case GL_DEPTH_TEST: return S.depthTest;
		case GL_CULL_FACE: return S.cullFace;
		case GL_STENCIL_TEST: return S.stencilTest;
		case GL_SCISSOR_TEST: return S.scissorTest;
		default: return GL_FALSE;
	}
}

void glcHint(GLenum target, GLenum mode) {
	if (target == GL_GENERATE_MIPMAP_HINT || target == GL_FRAGMENT_SHADER_DERIVATIVE_HINT)
		::glHint(target, mode);
}

void glcGetIntegerv(GLenum pname, GLint* data) {
	switch (pname) {
		case GL_MAX_LIGHTS: *data = kMaxLights; return;
		case GL_MAX_MODELVIEW_STACK_DEPTH:
		case GL_MAX_PROJECTION_STACK_DEPTH: *data = kMaxStackDepth; return;
		case GL_VIEWPORT: std::memcpy(data, S.viewport, sizeof(S.viewport)); return;
		case GL_TEXTURE_BINDING_2D: *data = (GLint)S.boundTexture; return;
		default: ::glGetIntegerv(pname, data); return;
	}
}

void glcGetFloatv(GLenum pname, GLfloat* data) {
	switch (pname) {
		case GL_MODELVIEW_MATRIX: std::memcpy(data, modelviewStack.back().m, 64); return;
		case GL_PROJECTION_MATRIX: std::memcpy(data, projectionStack.back().m, 64); return;
		case GL_CURRENT_COLOR: std::memcpy(data, S.color, 16); return;
		default: ::glGetFloatv(pname, data); return;
	}
}

void glcGetBooleanv(GLenum pname, GLboolean* data) {
	if (pname == GL_DOUBLEBUFFER) { *data = GL_TRUE; return; }
	::glGetBooleanv(pname, data);
}

const GLubyte* glcGetString(GLenum name) {
	if (name == GL_EXTENSIONS) return (const GLubyte*)"";
	const GLubyte* s = ::glGetString(name);
	return s ? s : (const GLubyte*)"";
}

void glcBindTexture(GLenum target, GLuint texture) {
	if (target == GL_TEXTURE_2D) S.boundTexture = texture;
	::glBindTexture(target, texture);
}

void glcDeleteTextures(GLsizei n, const GLuint* textures) {
	for (GLsizei i = 0; i < n; i++) {
		textureSizes.erase(textures[i]);
		if (S.boundTexture == textures[i]) S.boundTexture = 0;
		for (SavedState& s : savedStates)
			if (s.state.boundTexture == textures[i]) s.state.boundTexture = 0;
	}
	::glDeleteTextures(n, textures);
}

void glcTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
                   GLint border, GLenum format, GLenum type, const void* pixels) {
	if (target == GL_TEXTURE_2D && level == 0)
		textureSizes[S.boundTexture] = TexInfo{width, height};
	::glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glcGetTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) {
	auto it = textureSizes.find(S.boundTexture);
	if (it == textureSizes.end() || level != 0) { *params = 0; return; }
	if (pname == GL_TEXTURE_WIDTH) *params = it->second.w;
	else if (pname == GL_TEXTURE_HEIGHT) *params = it->second.h;
	else *params = 0;
}

void glcViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
	S.viewport[0] = x; S.viewport[1] = y; S.viewport[2] = width; S.viewport[3] = height;
	::glViewport(x, y, width, height);
}

void glcBlendFunc(GLenum sfactor, GLenum dfactor) {
	S.blendSrc = sfactor;
	S.blendDst = dfactor;
	::glBlendFunc(sfactor, dfactor);
}

void glcDepthFunc(GLenum func) {
	S.depthFunc = func;
	::glDepthFunc(func);
}

void glcDepthMask(GLboolean flag) {
	S.depthMask = flag != GL_FALSE;
	::glDepthMask(flag);
}

// --------------------------- matrices --------------------------------

void glcMatrixMode(GLenum mode) { S.matrixMode = mode; }

void glcLoadIdentity() { LoadMatrix(Mat4::Identity()); }

void glcPushMatrix() {
	std::vector<Mat4>& st = CurrentStack();
	if (st.size() < (std::size_t)kMaxStackDepth) st.push_back(st.back());
}

void glcPopMatrix() {
	std::vector<Mat4>& st = CurrentStack();
	if (st.size() > 1) st.pop_back();
	MatrixChanged();
}

void glcLoadMatrixf(const GLfloat* m) {
	Mat4 r;
	std::memcpy(r.m, m, sizeof(r.m));
	LoadMatrix(r);
}

void glcLoadMatrixd(const GLdouble* m) {
	Mat4 r;
	for (int i = 0; i < 16; i++) r.m[i] = (float)m[i];
	LoadMatrix(r);
}

void glcMultMatrixf(const GLfloat* m) {
	Mat4 r;
	std::memcpy(r.m, m, sizeof(r.m));
	MultMatrix(r);
}

void glcMultMatrixd(const GLdouble* m) {
	Mat4 r;
	for (int i = 0; i < 16; i++) r.m[i] = (float)m[i];
	MultMatrix(r);
}

void glcTranslatef(GLfloat x, GLfloat y, GLfloat z) {
	Mat4 r = Mat4::Identity();
	r.m[12] = x; r.m[13] = y; r.m[14] = z;
	MultMatrix(r);
}

void glcTranslated(GLdouble x, GLdouble y, GLdouble z) { glcTranslatef((float)x, (float)y, (float)z); }

void glcRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
	const float len = std::sqrt(x * x + y * y + z * z);
	if (len < 1e-12f) return;
	x /= len; y /= len; z /= len;
	const float rad = angle * 3.14159265358979323846f / 180.f;
	const float c = std::cos(rad), s = std::sin(rad), t = 1.f - c;
	Mat4 r = Mat4::Identity();
	r.m[0] = x * x * t + c;     r.m[4] = x * y * t - z * s; r.m[8] = x * z * t + y * s;
	r.m[1] = y * x * t + z * s; r.m[5] = y * y * t + c;     r.m[9] = y * z * t - x * s;
	r.m[2] = x * z * t - y * s; r.m[6] = y * z * t + x * s; r.m[10] = z * z * t + c;
	MultMatrix(r);
}

void glcRotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z) {
	glcRotatef((float)angle, (float)x, (float)y, (float)z);
}

void glcScalef(GLfloat x, GLfloat y, GLfloat z) {
	Mat4 r = Mat4::Identity();
	r.m[0] = x; r.m[5] = y; r.m[10] = z;
	MultMatrix(r);
}

void glcScaled(GLdouble x, GLdouble y, GLdouble z) { glcScalef((float)x, (float)y, (float)z); }

void glcOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
	Mat4 m = Mat4::Identity();
	m.m[0] = (float)(2.0 / (r - l));
	m.m[5] = (float)(2.0 / (t - b));
	m.m[10] = (float)(-2.0 / (f - n));
	m.m[12] = (float)(-(r + l) / (r - l));
	m.m[13] = (float)(-(t + b) / (t - b));
	m.m[14] = (float)(-(f + n) / (f - n));
	MultMatrix(m);
}

void glcFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) {
	Mat4 m;
	std::memset(m.m, 0, sizeof(m.m));
	m.m[0] = (float)(2.0 * n / (r - l));
	m.m[5] = (float)(2.0 * n / (t - b));
	m.m[8] = (float)((r + l) / (r - l));
	m.m[9] = (float)((t + b) / (t - b));
	m.m[10] = (float)(-(f + n) / (f - n));
	m.m[11] = -1.f;
	m.m[14] = (float)(-2.0 * f * n / (f - n));
	MultMatrix(m);
}

// ------------------------- immediate mode ----------------------------

void glcBegin(GLenum mode) {
	inBegin = true;
	immMode = mode;
	immVerts.clear();
}

void glcEnd() {
	inBegin = false;
	FlushImmediate(immMode);
}

void glcVertex4f(GLfloat x, GLfloat y, GLfloat z, GLfloat w) { if (inBegin) EmitVertex(x, y, z, w); }
void glcVertex2f(GLfloat x, GLfloat y) { glcVertex4f(x, y, 0.f, 1.f); }
void glcVertex2i(GLint x, GLint y) { glcVertex4f((float)x, (float)y, 0.f, 1.f); }
void glcVertex3f(GLfloat x, GLfloat y, GLfloat z) { glcVertex4f(x, y, z, 1.f); }
void glcVertex3d(GLdouble x, GLdouble y, GLdouble z) { glcVertex4f((float)x, (float)y, (float)z, 1.f); }
void glcVertex3fv(const GLfloat* v) { glcVertex4f(v[0], v[1], v[2], 1.f); }

void glcNormal3f(GLfloat x, GLfloat y, GLfloat z) { S.normal[0] = x; S.normal[1] = y; S.normal[2] = z; }
void glcNormal3d(GLdouble x, GLdouble y, GLdouble z) { glcNormal3f((float)x, (float)y, (float)z); }
void glcNormal3i(GLint x, GLint y, GLint z) { glcNormal3f((float)x, (float)y, (float)z); }
void glcNormal3fv(const GLfloat* v) { glcNormal3f(v[0], v[1], v[2]); }

void glcTexCoord2f(GLfloat s, GLfloat t) { S.texcoord[0] = s; S.texcoord[1] = t; }
void glcTexCoord2d(GLdouble s, GLdouble t) { glcTexCoord2f((float)s, (float)t); }
void glcTexCoord2i(GLint s, GLint t) { glcTexCoord2f((float)s, (float)t); }

void glcColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
	S.color[0] = r; S.color[1] = g; S.color[2] = b; S.color[3] = a;
}
void glcColor3f(GLfloat r, GLfloat g, GLfloat b) { glcColor4f(r, g, b, 1.f); }
void glcColor4fv(const GLfloat* v) { glcColor4f(v[0], v[1], v[2], v[3]); }
void glcColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
	glcColor4f(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
}
void glcColor3ub(GLubyte r, GLubyte g, GLubyte b) { glcColor4ub(r, g, b, 255); }
void glcColor4ubv(const GLubyte* v) { glcColor4ub(v[0], v[1], v[2], v[3]); }

// --------------------------- vertex arrays ---------------------------

static int ClientArrayIndex(GLenum array) {
	switch (array) {
		case GL_VERTEX_ARRAY: return A_VERTEX;
		case GL_COLOR_ARRAY: return A_COLOR;
		case GL_NORMAL_ARRAY: return A_NORMAL;
		case GL_TEXTURE_COORD_ARRAY: return A_TEXCOORD;
		default: return -1;
	}
}

void glcEnableClientState(GLenum array) {
	int i = ClientArrayIndex(array);
	if (i >= 0) S.arrays[i].enabled = true;
}

void glcDisableClientState(GLenum array) {
	int i = ClientArrayIndex(array);
	if (i >= 0) S.arrays[i].enabled = false;
}

static void SetPointer(int idx, GLint size, GLenum type, GLsizei stride, const void* ptr) {
	ClientArray& a = S.arrays[idx];
	a.size = size;
	a.type = type;
	a.stride = stride;
	a.ptr = ptr;
}

void glcVertexPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) { SetPointer(A_VERTEX, size, type, stride, ptr); }
void glcNormalPointer(GLenum type, GLsizei stride, const void* ptr) { SetPointer(A_NORMAL, 3, type, stride, ptr); }
void glcColorPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) { SetPointer(A_COLOR, size, type, stride, ptr); }
void glcTexCoordPointer(GLint size, GLenum type, GLsizei stride, const void* ptr) { SetPointer(A_TEXCOORD, size, type, stride, ptr); }

void glcDrawArrays(GLenum mode, GLint first, GLsizei count) {
	if (count <= 0 || !S.arrays[A_VERTEX].enabled) return;
	ApplyUniforms();
	UploadClientArrays((uint32_t)first, (uint32_t)(first + count - 1));
	DrawSequential(mode, (std::size_t)count);
}

void glcDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
	if (count <= 0 || !S.arrays[A_VERTEX].enabled || indices == nullptr) return;
	static std::vector<uint32_t> idx;
	idx.resize(count);
	uint32_t lo = UINT32_MAX, hi = 0;
	for (GLsizei i = 0; i < count; i++) {
		uint32_t v;
		if (type == GL_UNSIGNED_BYTE) v = ((const uint8_t*)indices)[i];
		else if (type == GL_UNSIGNED_SHORT) v = ((const uint16_t*)indices)[i];
		else v = ((const uint32_t*)indices)[i];
		idx[i] = v;
		if (v < lo) lo = v;
		if (v > hi) hi = v;
	}
	for (GLsizei i = 0; i < count; i++) idx[i] -= lo;

	ApplyUniforms();
	UploadClientArrays(lo, hi);
	const uint32_t range = hi - lo + 1;
	if (mode == GL_QUADS) {
		QuadsToTriangles(idx.data(), idx.size(), primIndices);
		DrawIndexed(GL_TRIANGLES, primIndices.data(), primIndices.size(), range);
	} else {
		std::size_t n = idx.size();
		if (mode == GL_QUAD_STRIP) n &= ~(std::size_t)1;
		DrawIndexed(NativeMode(mode), idx.data(), n, range);
	}
}

// --------------------------- fixed function --------------------------

void glcShadeModel(GLenum) {}  // smooth shading only

void glcAlphaFunc(GLenum func, GLclampf ref) {
	S.alphaFunc = func;
	S.alphaRef = ref;
}

void glcLightfv(GLenum light, GLenum pname, const GLfloat* params) {
	if (light < GL_LIGHT0 || light >= GL_LIGHT0 + kMaxLights) return;
	LightState& l = S.lights[light - GL_LIGHT0];
	switch (pname) {
		case GL_AMBIENT: std::memcpy(l.ambient, params, 16); break;
		case GL_DIFFUSE: std::memcpy(l.diffuse, params, 16); break;
		case GL_SPECULAR: std::memcpy(l.specular, params, 16); break;
		case GL_POSITION: TransformVec4(modelviewStack.back(), params, l.position); break;
		case GL_CONSTANT_ATTENUATION: l.attenuation[0] = params[0]; break;
		case GL_LINEAR_ATTENUATION: l.attenuation[1] = params[0]; break;
		case GL_QUADRATIC_ATTENUATION: l.attenuation[2] = params[0]; break;
		default: break;
	}
}

void glcLightf(GLenum light, GLenum pname, GLfloat param) { glcLightfv(light, pname, &param); }

void glcLightModelfv(GLenum pname, const GLfloat* params) {
	if (pname == GL_LIGHT_MODEL_AMBIENT) std::memcpy(S.lightModelAmbient, params, 16);
}

void glcLightModeli(GLenum, GLint) {}

void glcMaterialfv(GLenum face, GLenum pname, const GLfloat* params) {
	if (face == GL_BACK) return;  // two-sided lighting is not used
	switch (pname) {
		case GL_AMBIENT: std::memcpy(S.matAmbient, params, 16); break;
		case GL_DIFFUSE: std::memcpy(S.matDiffuse, params, 16); break;
		case GL_AMBIENT_AND_DIFFUSE:
			std::memcpy(S.matAmbient, params, 16);
			std::memcpy(S.matDiffuse, params, 16);
			break;
		case GL_SPECULAR: std::memcpy(S.matSpecular, params, 16); break;
		case GL_EMISSION: std::memcpy(S.matEmission, params, 16); break;
		case GL_SHININESS: S.matShininess = params[0]; break;
		default: break;
	}
}

void glcMaterialf(GLenum face, GLenum pname, GLfloat param) { glcMaterialfv(face, pname, &param); }

void glcColorMaterial(GLenum, GLenum) {}  // only GL_AMBIENT_AND_DIFFUSE (the default) is used

void glcFogf(GLenum pname, GLfloat param) {
	switch (pname) {
		case GL_FOG_MODE: S.fogMode = (GLenum)param; break;
		case GL_FOG_START: S.fogStart = param; break;
		case GL_FOG_END: S.fogEnd = param; break;
		case GL_FOG_DENSITY: S.fogDensity = param; break;
		default: break;
	}
}

void glcFogi(GLenum pname, GLint param) {
	if (pname == GL_FOG_MODE) S.fogMode = (GLenum)param;
	else glcFogf(pname, (float)param);
}

void glcFogfv(GLenum pname, const GLfloat* params) {
	if (pname == GL_FOG_COLOR) std::memcpy(S.fogColor, params, 16);
	else glcFogf(pname, params[0]);
}

void glcTexEnvi(GLenum target, GLenum pname, GLint param) {
	if (target == GL_TEXTURE_ENV && pname == GL_TEXTURE_ENV_MODE) S.texEnvMode = (GLenum)param;
}

void glcTexEnvf(GLenum target, GLenum pname, GLfloat param) { glcTexEnvi(target, pname, (GLint)param); }

void glcTexGeni(GLenum, GLenum, GLint) {}  // only GL_OBJECT_LINEAR is used

void glcTexGenfv(GLenum coord, GLenum pname, const GLfloat* params) {
	if (pname != GL_OBJECT_PLANE) return;
	if (coord == GL_S) std::memcpy(S.planeS, params, 16);
	else if (coord == GL_T) std::memcpy(S.planeT, params, 16);
}

// =====================================================================
// GLU
// =====================================================================

GLUquadric* gluNewQuadric() { return new GLUquadric(); }
void gluDeleteQuadric(GLUquadric* q) { delete q; }
void gluQuadricDrawStyle(GLUquadric* q, GLenum style) { q->drawStyle = style; }
void gluQuadricOrientation(GLUquadric* q, GLenum orientation) { q->orientation = orientation; }
void gluQuadricNormals(GLUquadric* q, GLenum normals) { q->normals = normals; }
void gluQuadricTexture(GLUquadric* q, GLboolean texture) { q->texture = texture != GL_FALSE; }

// Same tessellation as Mesa's GLU, emitted as a single triangle batch.
void gluSphere(GLUquadric* q, GLdouble radius, GLint slices, GLint stacks) {
	if (slices < 2 || stacks < 2) return;
	const float nsign = (q->orientation == GLU_INSIDE) ? -1.f : 1.f;
	const bool normals = q->normals != GLU_NONE;
	const bool texture = q->texture;
	const float r = (float)radius;
	const float drho = 3.14159265358979323846f / stacks;
	const float dtheta = 2.f * 3.14159265358979323846f / slices;

	auto vert = [&](float rho, float theta, float s, float t) {
		SphereVert v;
		const float x = -std::sin(theta) * std::sin(rho);
		const float y = std::cos(theta) * std::sin(rho);
		const float z = std::cos(rho);
		v.n[0] = x * nsign; v.n[1] = y * nsign; v.n[2] = z * nsign;
		v.p[0] = x * r; v.p[1] = y * r; v.p[2] = z * r;
		v.t[0] = s; v.t[1] = t;
		return v;
	};

	std::vector<SphereVert> strip;
	glcBegin(GL_TRIANGLES);
	const float ds = 1.f / slices, dt = 1.f / stacks;
	float t = 1.f;
	for (int i = 0; i < stacks; i++) {
		const float rho = i * drho;
		strip.clear();
		float s = 0.f;
		for (int j = 0; j <= slices; j++) {
			const float theta = (j == slices) ? 0.f : j * dtheta;
			// GLU_OUTSIDE emits (rho, rho+drho) pairs; GLU_INSIDE swaps them.
			if (nsign > 0) {
				strip.push_back(vert(rho, theta, s, t));
				strip.push_back(vert(rho + drho, theta, s, t - dt));
			} else {
				strip.push_back(vert(rho + drho, theta, s, t - dt));
				strip.push_back(vert(rho, theta, s, t));
			}
			s += ds;
		}
		// triangle strip -> triangles, preserving GL winding
		for (std::size_t k = 0; k + 2 < strip.size(); k++) {
			const SphereVert& a = strip[k];
			const SphereVert& b = strip[k + 1];
			const SphereVert& c = strip[k + 2];
			if (k % 2 == 0) { EmitSphereVert(a, normals, texture); EmitSphereVert(b, normals, texture); EmitSphereVert(c, normals, texture); }
			else { EmitSphereVert(b, normals, texture); EmitSphereVert(a, normals, texture); EmitSphereVert(c, normals, texture); }
		}
		t -= dt;
	}
	glcEnd();
}

void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
	const double f = 1.0 / std::tan(fovy * 3.14159265358979323846 / 360.0);
	Mat4 m;
	std::memset(m.m, 0, sizeof(m.m));
	m.m[0] = (float)(f / aspect);
	m.m[5] = (float)f;
	m.m[10] = (float)((zFar + zNear) / (zNear - zFar));
	m.m[11] = -1.f;
	m.m[14] = (float)(2.0 * zFar * zNear / (zNear - zFar));
	MultMatrix(m);
}

void gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
               GLdouble centerX, GLdouble centerY, GLdouble centerZ,
               GLdouble upX, GLdouble upY, GLdouble upZ) {
	double f[3] = {centerX - eyeX, centerY - eyeY, centerZ - eyeZ};
	double fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
	if (fl < 1e-12) return;
	for (double& c : f) c /= fl;
	double u[3] = {upX, upY, upZ};
	double s[3] = {f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0]};
	double sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
	if (sl < 1e-12) return;
	for (double& c : s) c /= sl;
	double uu[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
	Mat4 m = Mat4::Identity();
	m.m[0] = (float)s[0]; m.m[4] = (float)s[1]; m.m[8] = (float)s[2];
	m.m[1] = (float)uu[0]; m.m[5] = (float)uu[1]; m.m[9] = (float)uu[2];
	m.m[2] = (float)-f[0]; m.m[6] = (float)-f[1]; m.m[10] = (float)-f[2];
	MultMatrix(m);
	glcTranslated(-eyeX, -eyeY, -eyeZ);
}

const GLubyte* gluErrorString(GLenum error) {
	switch (error) {
		case GL_NO_ERROR: return (const GLubyte*)"no error";
		case GL_INVALID_ENUM: return (const GLubyte*)"invalid enumerant";
		case GL_INVALID_VALUE: return (const GLubyte*)"invalid value";
		case GL_INVALID_OPERATION: return (const GLubyte*)"invalid operation";
		case GL_OUT_OF_MEMORY: return (const GLubyte*)"out of memory";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return (const GLubyte*)"invalid framebuffer operation";
		default: return (const GLubyte*)"unknown error";
	}
}
