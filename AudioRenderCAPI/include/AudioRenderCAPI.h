#ifndef AUDIO_RENDER_C_API_H
#define AUDIO_RENDER_C_API_H

#include <stdint.h>

#ifdef AUDIO_RENDER_EXPORTS
#define AUDIO_RENDER_API __declspec(dllexport)
#else
#define AUDIO_RENDER_API __declspec(dllimport)
#endif

#if defined __cplusplus
extern "C" {
#endif

typedef int32_t audioRender_Bool;
typedef void audioRender_DrawDevice;

struct audioRender_Rectangle {
    float left;
    float top;
    float right;
    float bottom;
};

struct audioRender_Point {
    float x;
    float y;
};

// Create draw device which uses audio as output
AUDIO_RENDER_API audioRender_DrawDevice* audioRender_DeviceInitAudioRender(float scaleX, float scaleY);

// Create draw device which uses screen rendering as output
AUDIO_RENDER_API audioRender_DrawDevice* audioRender_DeviceInitScreenRender(
    const char* backgroundImagePath, audioRender_Bool simulateFlicker, audioRender_Bool simulateBeamIdle);

// Release draw device
AUDIO_RENDER_API void audioRender_DeviceFree(audioRender_DrawDevice* device);

// Returns when all data has been submitted to audio pipeline and more is needed
AUDIO_RENDER_API bool audioRender_WaitSync(audioRender_DrawDevice* device);

// Call to start drawing
AUDIO_RENDER_API void audioRender_Begin(audioRender_DrawDevice* device);

// Call to end drawing and submit graphics for rendering
AUDIO_RENDER_API void audioRender_Submit(audioRender_DrawDevice* device);

// Graphics primitives
AUDIO_RENDER_API struct audioRender_Rectangle audioRender_GetViewPort(audioRender_DrawDevice* device);

// set current draw point
AUDIO_RENDER_API void audioRender_SetPoint(audioRender_DrawDevice* device, const struct audioRender_Point* p);

// set intensity of next draw
AUDIO_RENDER_API void audioRender_SetIntensity(audioRender_DrawDevice* device, float intensity);

// draw circle on current point
AUDIO_RENDER_API void audioRender_DrawCircle(audioRender_DrawDevice* device, float radius);

// draw line from current point to target point. Target point becomes new current point.
// Line intensity will lerp linearnly towards intensity, if >.
AUDIO_RENDER_API void audioRender_DrawLine(audioRender_DrawDevice* device, const struct audioRender_Point* to, float intensity = -1);

#if defined __cplusplus
}
#endif
#endif  // AUDIO_RENDER_C_API_H
