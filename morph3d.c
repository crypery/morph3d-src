/*
 * Git: https://github.com/crypery
 * Author: https://crypery.com
 * License: GNU AGPL v3 (Affero GPL)
 */

#include <windows.h>
#include <gl/gl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#endif

// =====================================================================
//  Morph3D - screensaver: morphing outline figures made of points
//  Torus / sphere / dodecahedron / cube, soft neon,
//  trails, rotation, spiral motion, morphing + color gradient.
//  MULTIMONITOR: screensaver on each connected monitor.
//  Exit: mouse movement / pressing any key (on any monitor).
//  Arguments: /s (run), /c (settings). Compatible with C and C++.
// =====================================================================

// ---------- Exit on input ----------
// When a fullscreen window is created, Windows immediately sends WM_MOUSEMOVE
// (the cursor is already over the window) -> without a "quarantine" the screensaver would close instantly.
#define INPUT_GRACE_MS 2000      // ignore input for the first N ms after launch
#define MOUSE_MOVE_THRESHOLD 5   // real mouse movement threshold (pixels)
static DWORD startupTick = 0;

// ---------- Constants ----------
#define MAX_MONITORS 4        // maximum 4 monitors
#define NUM_POINTS   600      // same number of points in all shapes (6x10x10 cube, 30x20 torus, 30x20 dodeca: no holes)
#define SHAPE_SIZE   2.0f     // uniform size: diameter / maximum face
#define POINT_SIZE   16.0f    // point diameter (pixels)
#define NUM_SHAPES   4
#define CAM_DIST     5.0f     // camera-to-scene distance
#define FOV_DEG      45.0f
#define SPIRAL_SPEED 0.6f     // elliptical spiral phase speed (rad/s)
#define ROT_SPEED_DEG 40.0f   // rotation speed (deg/s), direction is fixed
#define TRAIL_LEN    5        // number of points in the fading trail
#define TRAIL_SAMPLE 4        // update the trail every N frames (long trail)

// ---------- Timings (seconds) ----------
#define STABLE_MIN   2.0f     // min. stable-state time
#define STABLE_MAX   6.0f     // max. stable-state time
#define MORPH_MIN    2.0f     // min. morphing time
#define MORPH_MAX    6.0f     // max. morphing time

enum { SHAPE_TORUS, SHAPE_SPHERE, SHAPE_DODECA, SHAPE_CUBE };
enum { STATE_STABLE, STATE_MORPH };

// ---------- Shared data (common to all monitors) ----------
static float shapePts[NUM_SHAPES][NUM_POINTS][3];

// "Soft neon" palette
static const float NEON_COLORS[][3] = {
    { 0.45f, 0.90f, 1.00f },  // cyan
    { 1.00f, 0.45f, 0.90f },  // magenta
    { 0.45f, 1.00f, 0.65f },  // green
    { 0.45f, 0.65f, 1.00f },  // blue
    { 1.00f, 0.55f, 0.85f },  // pink
    { 1.00f, 0.90f, 0.45f },  // yellow
    { 0.75f, 0.45f, 1.00f },  // purple
    { 1.00f, 0.65f, 0.35f },  // orange
};
#define NUM_COLORS (int)(sizeof(NEON_COLORS) / sizeof(NEON_COLORS[0]))

// ---------- Scene state (one per monitor) ----------
typedef struct {
    // Movement and bounds
    float center[3], vel[3];
    float bounds[3], boxCenter[3];
    float spiralPhase;
    // Rotation
    float rotAxis[3], rotAngle, rotSpeed;
    // Morphing and state machine
    int curShape, nextShape, prevShape;
    float morphT;
    float colorA[3], colorB[3], curColor[3];
    int curColorIdx, prevColorIdx, nextColorIdx;
    int state;
    float stateTime, stateDuration;
    int trailReset;
    float curPos[NUM_POINTS][3];
    // Trails / screen coordinates
    float scrX[NUM_POINTS], scrY[NUM_POINTS], scrSize[NUM_POINTS];
    float trailX[NUM_POINTS][TRAIL_LEN];
    float trailY[NUM_POINTS][TRAIL_LEN];
    float trailSz[NUM_POINTS][TRAIL_LEN];
    int trailHead[NUM_POINTS];
    int firstFrame;
    int frameCount;
} TScene;

typedef struct {
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
    int left, top, right, bottom;   // monitor rectangle
    int lastMouseX, lastMouseY, mouseInit;
    TScene scene;
} TMonitor;

static TMonitor monitors[MAX_MONITORS];
static int monitorCount = 0;

// ---------- Shape generation (shared) ----------
// Torus (donut): points on the surface, lies in the XZ plane
static void genTorus(float pts[NUM_POINTS][3])
{
    float R = SHAPE_SIZE * 0.32f;
    float r = SHAPE_SIZE * 0.18f;
    int nu = 30;
    int nv = NUM_POINTS / nu;
    int idx = 0;
    for (int i = 0; i < nu && idx < NUM_POINTS; i++) {
        float u = 2.0f * (float)M_PI * (float)i / (float)nu;
        for (int j = 0; j < nv && idx < NUM_POINTS; j++) {
            float v = 2.0f * (float)M_PI * (float)j / (float)nv;
            float cu = cosf(u), su = sinf(u);
            float cv = cosf(v), sv = sinf(v);
            pts[idx][0] = (R + r * cv) * cu;
            pts[idx][1] = r * sv;
            pts[idx][2] = (R + r * cv) * su;
            idx++;
        }
    }
    while (idx < NUM_POINTS) { memcpy(pts[idx], pts[0], 12); idx++; }
}

// Sphere: uniform distribution (Fibonacci spiral)
static void genSphere(float pts[NUM_POINTS][3])
{
    float radius = SHAPE_SIZE * 0.5f;
    float golden = (1.0f + sqrtf(5.0f)) * 0.5f;
    float step = 2.0f * (float)M_PI / golden;
    for (int i = 0; i < NUM_POINTS; i++) {
        float y = 1.0f - 2.0f * (float)i / (float)(NUM_POINTS - 1);
        float rad = sqrtf(1.0f - y * y);
        float th = step * (float)i;
        pts[i][0] = radius * rad * cosf(th);
        pts[i][1] = radius * y;
        pts[i][2] = radius * rad * sinf(th);
    }
}

// Dodecahedron: 20 vertices, 30 edges (edges found by minimum distance)
static void genDodeca(float pts[NUM_POINTS][3])
{
    float phi = (1.0f + sqrtf(5.0f)) * 0.5f;
    float invPhi = 1.0f / phi;
    float h = SHAPE_SIZE * 0.5f;
    float scale = h / phi;

    float v[20][3];
    int n = 0;
    for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++)
    for (int k = 0; k < 2; k++) {
        v[n][0] = (i ? 1 : -1) * scale;
        v[n][1] = (j ? 1 : -1) * scale;
        v[n][2] = (k ? 1 : -1) * scale;
        n++;
    }
    for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
        v[n][0] = 0;
        v[n][1] = (i ? invPhi : -invPhi) * scale;
        v[n][2] = (j ? phi : -phi) * scale;
        n++;
    }
    for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
        v[n][0] = (i ? invPhi : -invPhi) * scale;
        v[n][1] = (j ? phi : -phi) * scale;
        v[n][2] = 0;
        n++;
    }
    for (int i = 0; i < 2; i++)
    for (int j = 0; j < 2; j++) {
        v[n][0] = (i ? phi : -phi) * scale;
        v[n][1] = 0;
        v[n][2] = (j ? invPhi : -invPhi) * scale;
        n++;
    }

    float edgeLen = 2.0f / phi * scale;
    float tol = edgeLen * 0.1f;
    int edges[60][2];
    int edgeCount = 0;
    for (int i = 0; i < 20; i++) {
        for (int j = i + 1; j < 20; j++) {
            float dx = v[i][0] - v[j][0];
            float dy = v[i][1] - v[j][1];
            float dz = v[i][2] - v[j][2];
            float d = sqrtf(dx*dx + dy*dy + dz*dz);
            if (fabsf(d - edgeLen) < tol) {
                edges[edgeCount][0] = i;
                edges[edgeCount][1] = j;
                edgeCount++;
            }
        }
    }
    if (edgeCount < 1) edgeCount = 1;

    int perEdge = NUM_POINTS / edgeCount;
    int rem = NUM_POINTS % edgeCount;
    int idx = 0;
    for (int e = 0; e < edgeCount && idx < NUM_POINTS; e++) {
        int count = perEdge + (e < rem ? 1 : 0);
        for (int i = 0; i < count && idx < NUM_POINTS; i++) {
            float t = (float)i / (float)count;
            int a = edges[e][0], b = edges[e][1];
            pts[idx][0] = v[a][0] + (v[b][0] - v[a][0]) * t;
            pts[idx][1] = v[a][1] + (v[b][1] - v[a][1]) * t;
            pts[idx][2] = v[a][2] + (v[b][2] - v[a][2]) * t;
            idx++;
        }
    }
    while (idx < NUM_POINTS) { memcpy(pts[idx], pts[0], 12); idx++; }
}

// Cube: points at the intersections of a virtual NxN grid on each of the 6 faces.
// The grid covers the whole face, including corners (cube vertices).
static void genCube(float pts[NUM_POINTS][3])
{
    float h = SHAPE_SIZE * 0.5f;
    int N = (int)(sqrtf((float)NUM_POINTS / 6.0f) + 0.5f); // grid points per side (8)
    // 6 faces: center, two tangents (normal not needed)
    float fc[6][3] = {
        { h, 0, 0}, {-h, 0, 0},
        { 0, h, 0}, { 0,-h, 0},
        { 0, 0, h}, { 0, 0,-h},
    };
    float fu[6][3] = {
        {0, 1, 0}, {0, 1, 0},
        {1, 0, 0}, {1, 0, 0},
        {1, 0, 0}, {1, 0, 0},
    };
    float fv[6][3] = {
        {0, 0, 1}, {0, 0, 1},
        {0, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {0, 1, 0},
    };
    int idx = 0;
    for (int f = 0; f < 6 && idx < NUM_POINTS; f++) {
        for (int i = 0; i < N && idx < NUM_POINTS; i++) {
            for (int j = 0; j < N && idx < NUM_POINTS; j++) {
                // Grid intersections: from edge to edge of the face (including corners)
                float lx = ((float)i / (float)(N - 1) - 0.5f) * 2.0f * h;
                float ly = ((float)j / (float)(N - 1) - 0.5f) * 2.0f * h;
                pts[idx][0] = fc[f][0] + fu[f][0] * lx + fv[f][0] * ly;
                pts[idx][1] = fc[f][1] + fu[f][1] * lx + fv[f][1] * ly;
                pts[idx][2] = fc[f][2] + fu[f][2] * lx + fv[f][2] * ly;
                idx++;
            }
        }
    }
    while (idx < NUM_POINTS) { memcpy(pts[idx], pts[0], 12); idx++; }
}

static void InitShapes(void)
{
    genTorus(shapePts[SHAPE_TORUS]);
    genSphere(shapePts[SHAPE_SPHERE]);
    genDodeca(shapePts[SHAPE_DODECA]);
    genCube(shapePts[SHAPE_CUBE]);
}

// ---------- Motion ----------
// Motion bounds: the figure stays on screen (the center is clamped so that
// the whole figure remains within the monitor).
static void ComputeBounds(TScene* s, int winW, int winH)
{
    float fovRad = FOV_DEG * (float)M_PI / 180.0f;
    float focal = (winH * 0.5f) / tanf(fovRad * 0.5f);
    float maxR = 0.0f;
    for (int i = 0; i < NUM_POINTS; i++) {
        float r = sqrtf(shapePts[0][i][0]*shapePts[0][i][0] +
                        shapePts[0][i][1]*shapePts[0][i][1] +
                        shapePts[0][i][2]*shapePts[0][i][2]);
        if (r > maxR) maxR = r;
    }
    float screenR = maxR * focal / CAM_DIST;
    s->bounds[0] = (float)winW * 0.5f - screenR;
    s->bounds[1] = (float)winH * 0.5f - screenR;
    s->bounds[2] = 0.0f;
    if (s->bounds[0] < 0) s->bounds[0] = 0;
    if (s->bounds[1] < 0) s->bounds[1] = 0;
    s->boxCenter[0] = (float)winW * 0.5f;
    s->boxCenter[1] = (float)winH * 0.5f;
    s->boxCenter[2] = -CAM_DIST;
}

// Motion: a centered asymmetric elliptical spiral.
// The figure's center traces an ellipse around the screen center; the radius slowly
// pulsates (rf), so the trajectory is spiral-like and asymmetric.
// Depth is fixed: the figure neither approaches the camera nor recedes into the distance.
static void UpdateMotion(TScene* s, float dt, int winW, int winH)
{
    (void)winW; (void)winH;
    s->spiralPhase += SPIRAL_SPEED * dt;
    float rf = 0.5f + 0.5f * sinf(s->spiralPhase * 0.31f);
    s->center[0] = s->bounds[0] * 0.65f * rf * cosf(s->spiralPhase);
    s->center[1] = s->bounds[1] * 0.40f * rf * sinf(s->spiralPhase);
    s->center[2] = -CAM_DIST;
    s->rotAngle += s->rotSpeed * dt;
}

static void InitMotion(TScene* s)
{
    s->center[0] = 0.0f;
    s->center[1] = 0.0f;
    s->center[2] = 0.0f;
    s->spiralPhase = (float)rand() / (float)RAND_MAX * 2.0f * (float)M_PI;
}

static void InitRotation(TScene* s)
{
    s->rotAxis[0] = 0.35f;
    s->rotAxis[1] = 1.0f;
    s->rotAxis[2] = 0.25f;
    float len = sqrtf(s->rotAxis[0]*s->rotAxis[0] + s->rotAxis[1]*s->rotAxis[1] + s->rotAxis[2]*s->rotAxis[2]);
    s->rotAxis[0] /= len; s->rotAxis[1] /= len; s->rotAxis[2] /= len;
    s->rotAngle = 0.0f;
    s->rotSpeed = ROT_SPEED_DEG * (float)M_PI / 180.0f;
}
// ---------- Morphing ----------
static float randRange(float a, float b)
{
    return a + (float)rand() / (float)RAND_MAX * (b - a);
}

// New color: different from both the current and the previous one
static void StartMorph(TScene* s)
{
    int cand;
    do {
        cand = rand() % NUM_COLORS;
    } while (cand == s->curColorIdx || cand == s->prevColorIdx);
    s->nextColorIdx = cand;
    s->colorA[0] = s->curColor[0]; s->colorA[1] = s->curColor[1]; s->colorA[2] = s->curColor[2];
    s->colorB[0] = NEON_COLORS[s->nextColorIdx][0];
    s->colorB[1] = NEON_COLORS[s->nextColorIdx][1];
    s->colorB[2] = NEON_COLORS[s->nextColorIdx][2];
    s->prevShape = s->curShape;
    int candShape;
    do {
        candShape = rand() % NUM_SHAPES;
    } while (candShape == s->curShape || candShape == s->prevShape);
    s->nextShape = candShape;
}

// State machine: STABLE (2-6 s) -> MORPH (2-6 s) -> STABLE -> ...
static void UpdateState(TScene* s, float dt)
{
    s->stateTime += dt;
    if (s->stateTime >= s->stateDuration) {
        if (s->state == STATE_STABLE) {
            s->state = STATE_MORPH;
            s->stateTime = 0.0f;
            s->stateDuration = randRange(MORPH_MIN, MORPH_MAX);
            StartMorph(s);
            s->trailReset = 1;
        } else {
            s->prevColorIdx = s->curColorIdx;
            s->curColorIdx = s->nextColorIdx;
            s->prevShape = s->curShape;
            s->curShape = s->nextShape;
            s->state = STATE_STABLE;
            s->stateTime = 0.0f;
            s->stateDuration = randRange(STABLE_MIN, STABLE_MAX);
        }
    }

    float f = 0.0f;
    if (s->state == STATE_MORPH) {
        f = s->stateTime / s->stateDuration;
        if (f > 1.0f) f = 1.0f;
        float e = f * f * (3.0f - 2.0f * f);
        for (int i = 0; i < NUM_POINTS; i++) {
            s->curPos[i][0] = shapePts[s->curShape][i][0] + (shapePts[s->nextShape][i][0] - shapePts[s->curShape][i][0]) * e;
            s->curPos[i][1] = shapePts[s->curShape][i][1] + (shapePts[s->nextShape][i][1] - shapePts[s->curShape][i][1]) * e;
            s->curPos[i][2] = shapePts[s->curShape][i][2] + (shapePts[s->nextShape][i][2] - shapePts[s->curShape][i][2]) * e;
        }
        s->curColor[0] = s->colorA[0] + (s->colorB[0] - s->colorA[0]) * e;
        s->curColor[1] = s->colorA[1] + (s->colorB[1] - s->colorA[1]) * e;
        s->curColor[2] = s->colorA[2] + (s->colorB[2] - s->colorA[2]) * e;
    } else {
        for (int i = 0; i < NUM_POINTS; i++) {
            s->curPos[i][0] = shapePts[s->curShape][i][0];
            s->curPos[i][1] = shapePts[s->curShape][i][1];
            s->curPos[i][2] = shapePts[s->curShape][i][2];
        }
        s->curColor[0] = NEON_COLORS[s->curColorIdx][0];
        s->curColor[1] = NEON_COLORS[s->curColorIdx][1];
        s->curColor[2] = NEON_COLORS[s->curColorIdx][2];
    }
}

static void InitMorph(TScene* s)
{
    s->curShape = rand() % NUM_SHAPES;
    s->prevShape = -1;
    StartMorph(s);
    s->curColorIdx = s->nextColorIdx;
    s->prevColorIdx = -1;
    s->colorA[0] = NEON_COLORS[s->curColorIdx][0];
    s->colorA[1] = NEON_COLORS[s->curColorIdx][1];
    s->colorA[2] = NEON_COLORS[s->curColorIdx][2];
    s->colorB[0] = s->colorA[0]; s->colorB[1] = s->colorA[1]; s->colorB[2] = s->colorA[2];
    s->curColor[0] = s->colorA[0]; s->curColor[1] = s->colorA[1]; s->curColor[2] = s->colorA[2];
    s->morphT = 0.0f;
    s->state = STATE_STABLE;
    s->stateTime = 0.0f;
    s->stateDuration = randRange(STABLE_MIN, STABLE_MAX);
    for (int i = 0; i < NUM_POINTS; i++) {
        s->curPos[i][0] = shapePts[s->curShape][i][0];
        s->curPos[i][1] = shapePts[s->curShape][i][1];
        s->curPos[i][2] = shapePts[s->curShape][i][2];
    }
}
// ---------- Single-monitor rendering ----------
static void Render(TMonitor* m)
{
    TScene* s = &m->scene;
    int winW = m->right - m->left;
    int winH = m->bottom - m->top;
    if (winW < 1) winW = 1;
    if (winH < 1) winH = 1;

    glViewport(0, 0, winW, winH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float cr = s->curColor[0], cg = s->curColor[1], cb = s->curColor[2];
    float fovRad = FOV_DEG * (float)M_PI / 180.0f;
    float focal = (winH * 0.5f) / tanf(fovRad * 0.5f);

    float c = cosf(s->rotAngle), sf = sinf(s->rotAngle);
    float ax = s->rotAxis[0], ay = s->rotAxis[1], az = s->rotAxis[2];
    for (int p = 0; p < NUM_POINTS; p++) {
        float x = s->curPos[p][0];
        float y = s->curPos[p][1];
        float z = s->curPos[p][2];
        float dot = ax*x + ay*y + az*z;
        float cx = ay*z - az*y;
        float cy = az*x - ax*z;
        float cz = ax*y - ay*x;
        float rx = x*c + cx*sf + ax*dot*(1.0f - c);
        float ry = y*c + cy*sf + ay*dot*(1.0f - c);
        float rz = z*c + cz*sf + az*dot*(1.0f - c);
        // Depth: figure at the center, Z offset (presence effect) in world coordinates
        float ez = rz + s->center[2];
        if (ez >= -0.1f) { s->scrSize[p] = 0.0f; continue; }
        float d = -ez;
        // Projection (figure at the center), then X/Y offset in screen pixels
        s->scrX[p]    = winW * 0.5f + (rx / d) * focal + s->center[0];
        s->scrY[p]    = winH * 0.5f - (ry / d) * focal + s->center[1];
        s->scrSize[p] = POINT_SIZE * (CAM_DIST / d);
        if (s->scrSize[p] < 0.5f) s->scrSize[p] = 0.5f;
    }

    if (s->firstFrame) {
        for (int p = 0; p < NUM_POINTS; p++) {
            for (int k = 0; k < TRAIL_LEN; k++) {
                s->trailX[p][k] = s->scrX[p];
                s->trailY[p][k] = s->scrY[p];
                s->trailSz[p][k] = s->scrSize[p];
            }
            s->trailHead[p] = 0;
        }
        s->firstFrame = 0;
    }

    if (s->state == STATE_MORPH) {
        if (s->trailReset) {
            for (int p = 0; p < NUM_POINTS; p++) {
                for (int k = 0; k < TRAIL_LEN; k++) {
                    s->trailX[p][k] = s->scrX[p];
                    s->trailY[p][k] = s->scrY[p];
                    s->trailSz[p][k] = s->scrSize[p];
                }
                s->trailHead[p] = 0;
            }
            s->trailReset = 0;
        }
        s->frameCount++;
        if (s->frameCount % TRAIL_SAMPLE == 0) {
            for (int p = 0; p < NUM_POINTS; p++) {
                if (s->scrSize[p] <= 0.0f) continue;
                s->trailHead[p] = (s->trailHead[p] + 1) % TRAIL_LEN;
                s->trailX[p][s->trailHead[p]] = s->scrX[p];
                s->trailY[p][s->trailHead[p]] = s->scrY[p];
                s->trailSz[p][s->trailHead[p]] = s->scrSize[p];
            }
        }
    }

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_POINT_SMOOTH);

    // Trail (only while morphing)
    if (s->state == STATE_MORPH) {
        glBegin(GL_POINTS);
        for (int p = 0; p < NUM_POINTS; p++) {
            if (s->scrSize[p] <= 0.0f) continue;
            for (int k = 1; k < TRAIL_LEN; k++) {
                int idx = (s->trailHead[p] - k + TRAIL_LEN) % TRAIL_LEN;
                float f = (float)(TRAIL_LEN - k) / (float)TRAIL_LEN;
                float a = f * 0.9f;
                glPointSize(s->trailSz[p][idx] * (0.5f + 0.7f * f));
                glColor4f(cr * a, cg * a, cb * a, a);
                glVertex2f(s->trailX[p][idx], s->trailY[p][idx]);
            }
        }
        glEnd();
    }

    // Outer glow
    glBegin(GL_POINTS);
    for (int p = 0; p < NUM_POINTS; p++) {
        if (s->scrSize[p] <= 0.0f) continue;
        glPointSize(s->scrSize[p] * 10.0f);
        glColor4f(cr * 0.30f, cg * 0.30f, cb * 0.30f, 0.60f);
        glVertex2f(s->scrX[p], s->scrY[p]);
    }
    glEnd();

    // Inner glow
    glBegin(GL_POINTS);
    for (int p = 0; p < NUM_POINTS; p++) {
        if (s->scrSize[p] <= 0.0f) continue;
        glPointSize(s->scrSize[p] * 5.0f);
        glColor4f(cr * 0.55f, cg * 0.55f, cb * 0.55f, 1.0f);
        glVertex2f(s->scrX[p], s->scrY[p]);
    }
    glEnd();

    // Core
    glBegin(GL_POINTS);
    for (int p = 0; p < NUM_POINTS; p++) {
        if (s->scrSize[p] <= 0.0f) continue;
        glPointSize(s->scrSize[p]);
        glColor4f(cr, cg, cb, 1.0f);
        glVertex2f(s->scrX[p], s->scrY[p]);
    }
    glEnd();

    glDisable(GL_BLEND);
}

// ---------- Window procedure (input on any monitor terminates) ----------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    TMonitor* m = (TMonitor*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    int inGrace = ((GetTickCount() - startupTick) < INPUT_GRACE_MS);

    switch (message)
    {
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!inGrace) PostQuitMessage(0);
        return 0;
    case WM_MOUSEMOVE:
    {
        if (!inGrace && m) {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            if (!m->mouseInit) {
                m->lastMouseX = x; m->lastMouseY = y; m->mouseInit = 1;
            } else {
                int dx = x - m->lastMouseX, dy = y - m->lastMouseY;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx > MOUSE_MOVE_THRESHOLD || dy > MOUSE_MOVE_THRESHOLD)
                    PostQuitMessage(0);
                m->lastMouseX = x; m->lastMouseY = y;
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (!inGrace) PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

// ---------- OpenGL ----------
static void EnableOpenGL(HWND hWnd, HDC *hDC, HGLRC *hRC)
{
    *hDC = GetDC(hWnd);
    PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(*hDC, &pfd);
    SetPixelFormat(*hDC, pf, &pfd);
    *hRC = wglCreateContext(*hDC);
    wglMakeCurrent(*hDC, *hRC);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_POINT_SMOOTH);
}

static void DisableOpenGL(HWND hWnd, HDC hDC, HGLRC hRC)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
}

// ---------- Monitor enumeration ----------
static RECT enumRects[MAX_MONITORS];
static int enumCount = 0;

static BOOL CALLBACK EnumMonitorsProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprc, LPARAM lParam)
{
    (void)hMonitor; (void)hdcMonitor; (void)lParam;
    if (enumCount < MAX_MONITORS) {
        enumRects[enumCount] = *lprc;
        enumCount++;
    }
    return TRUE;
}

// ---------- Entry point ----------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
    (void)hPrevInstance; (void)iCmdShow;

    // Screensaver arguments
    char cmd[1024];
    strcpy(cmd, lpCmdLine);
    char *start = cmd;
    char *end = cmd + strlen(cmd);
    while (*start == ' ' || *start == '"') start++;
    while (end > start && (end[-1] == ' ' || end[-1] == '"')) end--;
    *end = '\0';

    if (start[0] == '/' && (start[1] == 'c' || start[1] == 'C')) {
        MessageBoxA(NULL, "Morph3D - no settings", "Morph3D", MB_OK);
        return 0;
    }

    WNDCLASS wc;
    memset(&wc, 0, sizeof(wc));
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "Morph3D";
    if (!RegisterClass(&wc)) return 0;

    // Enumerate all monitors
    enumCount = 0;
    EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, 0);
    monitorCount = enumCount;
    if (monitorCount < 1) monitorCount = 1;

    srand((unsigned)GetTickCount());
    InitShapes();

    // Create a window + GL context + scene on each monitor
    for (int i = 0; i < monitorCount; i++) {
        TMonitor* m = &monitors[i];
        m->left = enumRects[i].left;
        m->top = enumRects[i].top;
        m->right = enumRects[i].right;
        m->bottom = enumRects[i].bottom;
        m->mouseInit = 0;
        int w = m->right - m->left;
        int h = m->bottom - m->top;
        m->hWnd = CreateWindowEx(WS_EX_TOPMOST, "Morph3D", "Morph3D", WS_POPUP,
            m->left, m->top, w, h, NULL, NULL, hInstance, NULL);
        SetWindowLongPtr(m->hWnd, GWLP_USERDATA, (LONG_PTR)m);
        ShowWindow(m->hWnd, SW_SHOW);
        SetWindowPos(m->hWnd, HWND_TOPMOST, m->left, m->top, w, h, SWP_SHOWWINDOW);
        UpdateWindow(m->hWnd);
        EnableOpenGL(m->hWnd, &m->hDC, &m->hRC);
        ComputeBounds(&m->scene, w, h);
        InitMotion(&m->scene);
        InitRotation(&m->scene);
        InitMorph(&m->scene);
    }
    ShowCursor(FALSE);
    startupTick = GetTickCount();

    MSG msg;
    BOOL quit = FALSE;
    DWORD lastTime = GetTickCount();
    while (!quit)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) quit = TRUE;
            else { TranslateMessage(&msg); DispatchMessage(&msg); }
        }
        else
        {
            DWORD now = GetTickCount();
            float dt = (now - lastTime) / 1000.0f;
            lastTime = now;
            if (dt > 0.1f) dt = 0.1f;
            if (dt < 0.0f) dt = 0.0f;

            for (int i = 0; i < monitorCount; i++) {
                TMonitor* m = &monitors[i];
                int w = m->right - m->left;
                int h = m->bottom - m->top;
                wglMakeCurrent(m->hDC, m->hRC);
                UpdateMotion(&m->scene, dt, w, h);
                UpdateState(&m->scene, dt);
                Render(m);
                SwapBuffers(m->hDC);
            }

            DWORD frameEnd = GetTickCount();
            DWORD elapsed = frameEnd - now;
            if (elapsed < 16) Sleep(16 - elapsed);
        }
    }
    ShowCursor(TRUE);

    for (int i = 0; i < monitorCount; i++) {
        DisableOpenGL(monitors[i].hWnd, monitors[i].hDC, monitors[i].hRC);
        DestroyWindow(monitors[i].hWnd);
    }
    return (int)msg.wParam;
}
