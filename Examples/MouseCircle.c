#include "Common.h"

static SDL_GPUGraphicsPipeline* Pipeline;

typedef struct MouseCircleUniforms
{
    float mouse_x;
    float mouse_y;
    float resolution_x;
    float resolution_y;
} MouseCircleUniforms;

static MouseCircleUniforms UniformValues;

static int Init(Context* context)
{
    int result = CommonInit(context, 0);
    if (result < 0)
    {
        return result;
    }

    // Create the shaders
    SDL_GPUShader* vertexShader = LoadShader(context->Device, "MouseCircle.vert", 0, 0, 0, 0);
    if (vertexShader == NULL)
    {
        SDL_Log("Failed to create vertex shader!");
        return -1;
    }

    SDL_GPUShader* fragmentShader = LoadShader(context->Device, "MouseCircle.frag", 0, 1, 0, 0);
    if (fragmentShader == NULL)
    {
        SDL_Log("Failed to create fragment shader!");
        return -1;
    }

    // Create the pipeline
    SDL_GPUGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = SDL_GetGPUSwapchainTextureFormat(context->Device, context->Window)
            }},
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .vertex_shader = vertexShader,
        .fragment_shader = fragmentShader,
    };

    Pipeline = SDL_CreateGPUGraphicsPipeline(context->Device, &pipelineCreateInfo);
    if (Pipeline == NULL)
    {
        SDL_Log("Failed to create pipeline!");
        return -1;
    }

    // Clean up shader resources
    SDL_ReleaseGPUShader(context->Device, vertexShader);
    SDL_ReleaseGPUShader(context->Device, fragmentShader);

    // Initialize uniform values
    int width, height;
    SDL_GetWindowSizeInPixels(context->Window, &width, &height);
    UniformValues.mouse_x = width / 2.0f;
    UniformValues.mouse_y = height / 2.0f;
    UniformValues.resolution_x = (float)width;
    UniformValues.resolution_y = (float)height;

    SDL_Log("Move the mouse to see the circle follow!");

    return 0;
}

static int Update(Context* context)
{
    // Update mouse position
    float mouse_x, mouse_y;
    SDL_GetMouseState(&mouse_x, &mouse_y);
    UniformValues.mouse_x = mouse_x;
    UniformValues.mouse_y = mouse_y;

    // Update resolution in case window was resized
    int width, height;
    SDL_GetWindowSizeInPixels(context->Window, &width, &height);
    UniformValues.resolution_x = (float)width;
    UniformValues.resolution_y = (float)height;

    return 0;
}

static int Draw(Context* context)
{
    SDL_GPUCommandBuffer* cmdbuf = SDL_AcquireGPUCommandBuffer(context->Device);
    if (cmdbuf == NULL)
    {
        SDL_Log("AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return -1;
    }

    SDL_GPUTexture* swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, context->Window, &swapchainTexture, NULL, NULL)) {
        SDL_Log("WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return -1;
    }

    if (swapchainTexture != NULL)
    {
        SDL_GPUColorTargetInfo colorTargetInfo = { 0 };
        colorTargetInfo.texture = swapchainTexture;
        colorTargetInfo.clear_color = (SDL_FColor){ 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorTargetInfo, 1, NULL);
        SDL_BindGPUGraphicsPipeline(renderPass, Pipeline);
        SDL_PushGPUFragmentUniformData(cmdbuf, 0, &UniformValues, sizeof(MouseCircleUniforms));
        SDL_DrawGPUPrimitives(renderPass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(renderPass);
    }

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    return 0;
}

static void Quit(Context* context)
{
    SDL_ReleaseGPUGraphicsPipeline(context->Device, Pipeline);
    CommonQuit(context);
}

Example MouseCircle_Example = { "MouseCircle", Init, Update, Draw, Quit };
