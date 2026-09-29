# Morph3D Screensaver

> [Russian version](README_RU.md)

Morph3D is a full-screen Windows screensaver that displays animated contour figures made of glowing points. The figures continuously rotate, drift across the screen, change shape, and transition through a soft neon color palette. The visual style is minimal and abstract: dark background, luminous point clouds, smooth morphing, subtle motion trails, and a slow “presence” effect that makes the figure approach and recede in depth.

The project is designed as a standalone screensaver application with multi-monitor support. When started, it covers all connected displays with borderless top-most windows, hides the cursor, and exits on user input. Each monitor receives an independent animation instance, so the screensaver looks alive and non-repetitive across a multi-display desktop.

## Overview

Morph3D renders a set of abstract 3D shapes as point clouds. All shapes use the same number of points, which allows direct point-to-point morphing from one figure to another. During a stable period, the current shape rotates and moves smoothly. During a morphing period, the points interpolate toward the next shape while the color gradually changes to a new neon tone.

The animation is procedural. There are no video assets, textures, or external media. The visual result is generated in real time from mathematical shape definitions, rotation, projection, motion paths, color interpolation, and additive blending.

The screensaver is intended to be visually calm but active: it is not a fast particle system or a busy animation. The motion is slow, the color changes are soft, and the depth oscillation is subtle. The goal is an ambient desktop effect that is interesting to watch but not distracting.

## Key Features

- Full-screen screensaver mode for Windows.
- Multi-monitor support: every connected monitor receives its own full-screen animation window.
- Independent animation state per monitor.
- Borderless top-most windows without visible frame decorations.
- Cursor is hidden while the screensaver is running.
- Exits on keyboard input, mouse button press, or significant mouse movement on any monitor.
- Short input grace period after startup to prevent immediate exit from initial system input events.
- Procedural 3D point-cloud figures.
- Smooth shape morphing between multiple geometric forms.
- Soft neon color palette with gradual color transitions.
- Additive glow rendering for a luminous point effect.
- Motion trails during shape transitions.
- Slow elliptical spiral screen movement.
- Subtle depth oscillation for a sense of presence.
- Continuous rotation around a fixed normalized axis.
- C and C++ compatible source code.
- Build support for MinGW GCC and G++.
- Installer build support using Inno Setup.
- Windows manifest with per-monitor DPI awareness.
- Embedded application icon and version information.

## Visual Style

The screensaver uses a black background and bright saturated point colors. The palette is based on soft neon tones: cyan, magenta, green, blue, pink, yellow, purple, and orange. Colors are chosen randomly, but the next color is not allowed to be the same as the current or previous color, which helps keep the sequence visually varied.

Each figure is rendered as a cloud of points rather than solid polygons. The point rendering uses several layered passes:

- A large low-intensity outer glow.
- A medium inner glow.
- A small bright core.

These layers are drawn with additive blending, producing a neon-like luminous effect without requiring post-processing shaders.

During morphing, the application also draws fading trails behind the moving points. The trails are sampled periodically and stored in a short history buffer. They are rendered with decreasing intensity and size, creating a smooth comet-like effect while the shape changes.

## Animation Model

The animation combines several independent motion systems:

1. **Shape rotation**  
   The figure rotates continuously in 3D space. The rotation axis is fixed and normalized. The rotation speed is constant, producing steady orbital motion of the point cloud.

2. **Screen-space drift**  
   The center of the figure moves along a centered asymmetric elliptical spiral. The radius of the path slowly pulses, so the motion is not a perfect circle or ellipse. This creates a natural drifting pattern across the monitor.

3. **Depth oscillation**  
   The figure slowly moves toward and away from the camera along the depth axis. The movement is bounded so the figure does not cross the camera or disappear unexpectedly. This creates a subtle “presence” effect.

4. **Shape morphing**  
   The application alternates between stable and morphing states. In the stable state, the current shape is shown. In the morphing state, the current points interpolate toward the next shape. The interpolation uses a smoothstep easing curve, which makes transitions start slowly, accelerate in the middle, and finish smoothly.

5. **Color transition**  
   Color changes are synchronized with shape morphing. The current color blends into the next color using the same easing curve, producing a gradual neon shift rather than an abrupt palette change.

## Shape and Morphing Method

All supported shapes are generated as point clouds with an identical point count. This is the central method that makes morphing simple and stable: each point in the current shape has a direct corresponding point in the next shape.

Because of this, morphing does not require complex matching, deformation fields, or particle rebinding. The application simply interpolates between corresponding 3D positions.

The supported figure types include abstract geometric forms such as torus-like, sphere-like, dodecahedron-like, cube-like contours. The exact visual result depends on point distribution and projection, but the overall effect is a set of recognizable abstract 3D outlines.

The state machine uses randomized durations for both stable and morphing periods. This prevents the screensaver from feeling mechanical. The next shape and next color are also selected randomly, with constraints that avoid repeating the immediately previous visual state.

## Multi-Monitor Method

At startup, the application enumerates all displays attached to the system. For each display, it creates a separate full-screen window covering that monitor’s exact rectangle. Each window has its own device context, OpenGL context, input state, and animation state.

This design provides several benefits:

- Each monitor can show a different shape, color, rotation phase, and motion phase.
- One monitor’s rendering does not depend on another monitor’s state.
- Input on any monitor can stop the screensaver.
- The application adapts to common multi-monitor configurations without requiring user configuration.

The windows are created as borderless pop-up windows and marked top-most so they cover normal desktop content. The cursor is hidden after the windows are shown.

The current build uses the system-provided `MAX_MONITORS` limit from the Windows DDE header. In the current MinGW/Windows header environment this value is 4, so the practical multi-monitor limit is four displays.

## User Interaction

The screensaver exits when the user interacts with the system:

- Any key press.
- System key press.
- Left, right, or middle mouse button press.
- Mouse movement larger than a small threshold.

A short grace period is applied immediately after startup. This is important because full-screen window creation can generate initial mouse-move messages even when the user has not physically moved the mouse. Without the grace period, the screensaver could close immediately.

The mouse movement threshold is intentionally small, but large enough to ignore minor input jitter. Once real user interaction is detected, the application quits and restores the cursor.

## Technology Stack

Morph3D is built using classic Windows desktop technologies:

- **Win32 API** for application entry, window creation, message processing, monitor enumeration, cursor control, and input handling.
- **OpenGL legacy fixed-function pipeline** for 3D point rendering, projection, blending, and double-buffered presentation.
- **C/C++ compatible source code** so the project can be compiled as C or C++.
- **MinGW-w64 GCC/G++** for native Windows builds.
- **Windows resource compilation** for icon, manifest, and version information.
- **Inno Setup** for installer creation.
- **Windows manifest** for DPI awareness, common controls, and application metadata.

The rendering uses the immediate-mode OpenGL API and point primitives. This keeps the implementation compact and portable across standard Windows OpenGL environments. The visual effect relies on point size, color, alpha, and additive blending rather than modern shader-based post-processing.

## Rendering Method

The rendering pipeline is intentionally simple:

1. Clear the screen to black.
2. Update the animation state for the current frame.
3. Transform each 3D point by the current rotation.
4. Apply perspective projection using a fixed vertical field of view.
5. Add screen-space motion offsets.
6. Scale point size by depth.
7. Draw glow layers and core points with additive blending.
8. Swap front and back buffers.

The projection uses a standard pinhole-camera model. The focal length is derived from the monitor height and field of view. Points closer to the camera are projected larger, while points farther away are projected smaller. The depth offset used by the presence effect changes the apparent scale of the whole figure.

Double buffering is used to avoid flicker. Each monitor has its own OpenGL context, and the active context is selected before each monitor is rendered.

## Frame Timing

The main loop targets approximately 60 frames per second. After rendering, the application sleeps for the remaining time in the frame budget. Delta time is clamped to avoid large animation jumps after pauses, system delays, or temporary performance spikes.

This keeps the animation smooth under normal conditions and prevents unstable behavior if a frame takes longer than expected.

## Build System

The project is built with MinGW-w64 on Windows. The default build uses G++, but the source is also compatible with GCC as a C compiler.

Typical build steps:

1. Compile the Windows resource file.
2. Compile the application source.
3. Link against OpenGL, GDI, and common control libraries.
4. Produce the screensaver executable.
5. Optionally create an installer package with Inno Setup.

The build environment expects the MinGW-w64 tools to be available, including the C/C++ compiler and Windows resource compiler.

## Installer

The installer is generated with Inno Setup. It packages the screensaver executable and registers it as a Windows screensaver. The installer also creates standard uninstall information.

The installer is intended for local deployment on Windows machines. It uses administrator privileges where required for system screensaver registration.

## Application Metadata

The application includes embedded metadata:

- Application icon.
- Version information.
- Windows manifest.
- Per-monitor DPI awareness.
- Standard common controls support.
- As-invoker user access level.

The manifest ensures that the screensaver renders correctly on high-DPI displays and behaves as a modern Windows desktop application.

## Design Decisions

Several design choices define the project:

- **Point clouds instead of meshes**  
  Point clouds are simpler to morph and produce an abstract neon aesthetic. They also avoid the need for texture mapping, lighting models, or complex geometry management.

- **Fixed point count**  
  Using the same number of points for every shape makes morphing deterministic and efficient.

- **Legacy OpenGL**  
  The fixed-function pipeline is sufficient for this visual style and keeps the code compact and easy to build on standard Windows toolchains.

- **Additive blending instead of shaders**  
  Additive blending provides a convincing glow effect with minimal complexity.

- **Per-monitor state**  
  Independent state per monitor makes multi-display output more interesting and avoids synchronized repetition.

- **Procedural animation**  
  The screensaver does not depend on external assets, making it small, portable, and easy to embed in an installer.

- **Simple input exit logic**  
  A screensaver must reliably stop when the user returns. The input grace period and movement threshold balance responsiveness with stability.

## Limitations

- The number of supported monitors is limited by the `MAX_MONITORS` constant from the Windows header, currently 4 in the target build environment.
- There is no settings dialog. The visual behavior is fixed by built-in constants.
- There is no embedded preview-window mode.
- The renderer uses legacy OpenGL immediate mode, which is intentional for simplicity but not a modern GPU architecture.
- The animation is procedural and randomized, but it does not persist user preferences or custom configurations.

## Intended Use

Morph3D is suitable for:

- Desktop screensavers.
- Ambient displays.
- Multi-monitor installations.
- Demo screens.
- Minimal neon-style visual backgrounds.
- Educational examples of Win32 + OpenGL screensaver development.

The project demonstrates how to combine classic Windows programming, OpenGL rendering, procedural animation, multi-monitor handling, resource embedding, and installer packaging into a compact standalone screensaver.
