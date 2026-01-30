#ifndef GPU_RENDERER_H
#define GPU_RENDERER_H

#include "RenderConfig.h"
#include "Camera.h"
#include "Scene.h"
#include <vector>

/**
 * GPU-backed rendering using Metal. Intended as a simplified,
 * direct-lighting-only renderer for Apple Silicon.
 */
std::vector<unsigned char> render_image_gpu(const RenderConfig& config,
                                            const Camera& camera,
                                            const Scene& scene,
                                            int max_depth,
                                            double* gpu_ms = nullptr);

#endif
