/**
 * This is an extension of SDL3 for WebGPU, abstracting away the details of
 * OS-specific operations.
 * 
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 * 
 * MIT License
 * Copyright (c) 2022-2024 Elie Michel
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define SDL_MAIN_HANDLED
#include "sdl3webgpu.h"
#include <SDL3/SDL.h>
#include <webgpu/webgpu.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) (void)(x)
#define LOG_PREFIX "[triangle-sdl3]"

struct demo {
	WGPUInstance instance;
	WGPUSurface surface;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUSurfaceConfiguration config;
};

struct uniforms_data {
	float mouse_x;
	float mouse_y;
	float resolution_x;
	float resolution_y;
};

static WGPUShaderModule load_shader_module(WGPUDevice device, const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		printf(LOG_PREFIX " Failed to open shader file: %s\n", filename);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *shader_code = (char *)malloc(file_size + 1);
	if (!shader_code) {
		fclose(file);
		return NULL;
	}

	fread(shader_code, 1, file_size, file);
	shader_code[file_size] = '\0';
	fclose(file);

	WGPUShaderSourceWGSL wgsl_desc = {
		.chain = {
			.sType = WGPUSType_ShaderSourceWGSL,
		},
		.code = {shader_code, WGPU_STRLEN},
	};

	WGPUShaderModuleDescriptor shader_desc = {
		.nextInChain = (const WGPUChainedStruct *)&wgsl_desc,
		.label = {filename, WGPU_STRLEN},
	};

	WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &shader_desc);
	free(shader_code);
	return shader_module;
}

static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, WGPUStringView message,
                                   void *userdata1, void *userdata2) {
	UNUSED(userdata2);
	if (status == WGPURequestAdapterStatus_Success) {
		struct demo *d = userdata1;
		d->adapter = adapter;
	} else {
		printf(LOG_PREFIX " request_adapter status=%#.8x message=%.*s\n", status,
		       (int)message.length, message.data);
	}
}

static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device, WGPUStringView message,
                                  void *userdata1, void *userdata2) {
	UNUSED(userdata2);
	if (status == WGPURequestDeviceStatus_Success) {
		struct demo *d = userdata1;
		d->device = device;
	} else {
		printf(LOG_PREFIX " request_device status=%#.8x message=%.*s\n", status,
		       (int)message.length, message.data);
	}
}

int main(int argc, char* argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	// Init SDL
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		printf(LOG_PREFIX " Failed to initialize SDL: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}

	// Create demo struct
	struct demo demo = {0};

	// Init WebGPU
	demo.instance = wgpuCreateInstance(NULL);
	assert(demo.instance);

	// Create SDL window
	SDL_Window *window = SDL_CreateWindow("triangle [wgpu-native + SDL3]", 640, 480, 0);
	assert(window);

	// Create WebGPU surface from SDL window
	demo.surface = SDL_GetWGPUSurface(demo.instance, window);
	assert(demo.surface);

	// Request adapter
	wgpuInstanceRequestAdapter(demo.instance,
	                           &(const WGPURequestAdapterOptions){
	                               .compatibleSurface = demo.surface,
	                           },
	                           (const WGPURequestAdapterCallbackInfo){
	                               .callback = handle_request_adapter,
	                               .userdata1 = &demo
	                           });
	assert(demo.adapter);

	// Request device
	wgpuAdapterRequestDevice(demo.adapter, NULL,
	                         (const WGPURequestDeviceCallbackInfo){
	                             .callback = handle_request_device,
	                             .userdata1 = &demo
	                         });
	assert(demo.device);

	// Get queue
	WGPUQueue queue = wgpuDeviceGetQueue(demo.device);
	assert(queue);

	// Load shader
	WGPUShaderModule shader_module = load_shader_module(demo.device, "shader.wgsl");
	assert(shader_module);

	// Create uniform buffer
	WGPUBuffer uniform_buffer = wgpuDeviceCreateBuffer(
	    demo.device, &(const WGPUBufferDescriptor){
	                     .label = {"uniform_buffer", WGPU_STRLEN},
	                     .usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst,
	                     .size = sizeof(struct uniforms_data),
	                     .mappedAtCreation = false,
	                 });
	assert(uniform_buffer);

	// Create bind group layout
	WGPUBindGroupLayout bind_group_layout = wgpuDeviceCreateBindGroupLayout(
	    demo.device,
	    &(const WGPUBindGroupLayoutDescriptor){
	        .label = {"bind_group_layout", WGPU_STRLEN},
	        .entryCount = 1,
	        .entries =
	            (const WGPUBindGroupLayoutEntry[]){
	                (const WGPUBindGroupLayoutEntry){
	                    .binding = 0,
	                    .visibility = WGPUShaderStage_Fragment,
	                    .buffer =
	                        (const WGPUBufferBindingLayout){
	                            .type = WGPUBufferBindingType_Uniform,
	                            .minBindingSize = sizeof(struct uniforms_data),
	                        },
	                },
	            },
	    });
	assert(bind_group_layout);

	// Create bind group
	WGPUBindGroup bind_group = wgpuDeviceCreateBindGroup(
	    demo.device, &(const WGPUBindGroupDescriptor){
	                     .label = {"bind_group", WGPU_STRLEN},
	                     .layout = bind_group_layout,
	                     .entryCount = 1,
	                     .entries =
	                         (const WGPUBindGroupEntry[]){
	                             (const WGPUBindGroupEntry){
	                                 .binding = 0,
	                                 .buffer = uniform_buffer,
	                                 .offset = 0,
	                                 .size = sizeof(struct uniforms_data),
	                             },
	                         },
	                 });
	assert(bind_group);

	// Create pipeline layout
	WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(
	    demo.device, &(const WGPUPipelineLayoutDescriptor){
	                     .label = {"pipeline_layout", WGPU_STRLEN},
	                     .bindGroupLayoutCount = 1,
	                     .bindGroupLayouts = (const WGPUBindGroupLayout[]){bind_group_layout},
	                 });
	assert(pipeline_layout);

	// Get surface capabilities
	WGPUSurfaceCapabilities surface_capabilities = {0};
	wgpuSurfaceGetCapabilities(demo.surface, demo.adapter, &surface_capabilities);

	// Create render pipeline
	WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(
	    demo.device,
	    &(const WGPURenderPipelineDescriptor){
	        .label = {"render_pipeline", WGPU_STRLEN},
	        .layout = pipeline_layout,
	        .vertex =
	            (const WGPUVertexState){
	                .module = shader_module,
	                .entryPoint = {"vs_main", WGPU_STRLEN},
	            },
	        .fragment =
	            &(const WGPUFragmentState){
	                .module = shader_module,
	                .entryPoint = {"fs_main", WGPU_STRLEN},
	                .targetCount = 1,
	                .targets =
	                    (const WGPUColorTargetState[]){
	                        (const WGPUColorTargetState){
	                            .format = surface_capabilities.formats[0],
	                            .writeMask = WGPUColorWriteMask_All,
	                        },
	                    },
	            },
	        .primitive =
	            (const WGPUPrimitiveState){
	                .topology = WGPUPrimitiveTopology_TriangleList,
	            },
	        .multisample =
	            (const WGPUMultisampleState){
	                .count = 1,
	                .mask = 0xFFFFFFFF,
	            },
	    });
	assert(render_pipeline);

	// Configure surface
	demo.config = (const WGPUSurfaceConfiguration){
	    .device = demo.device,
	    .usage = WGPUTextureUsage_RenderAttachment,
	    .format = surface_capabilities.formats[0],
	    .presentMode = WGPUPresentMode_Fifo,
	    .alphaMode = surface_capabilities.alphaModes[0],
	};

	// Get window size and set surface size
	int width, height;
	SDL_GetWindowSize(window, &width, &height);
	demo.config.width = width;
	demo.config.height = height;

	wgpuSurfaceConfigure(demo.surface, &demo.config);

	// Mouse position tracking
	float mouse_x = width / 2.0f;
	float mouse_y = height / 2.0f;

	// FPS tracking
	Uint64 last_time = SDL_GetTicks();
	Uint64 last_fps_print = last_time;
	int frame_count = 0;

	// Main loop
	SDL_Event event;
	bool running = true;
	while (running) {
		// Handle events
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			} else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				// Handle window resize
				SDL_GetWindowSize(window, &width, &height);
				if (width != 0 && height != 0) {
					demo.config.width = width;
					demo.config.height = height;
					wgpuSurfaceConfigure(demo.surface, &demo.config);
				}
			} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
				// Track mouse position
				mouse_x = event.motion.x;
				mouse_y = event.motion.y;
			}
		}

		// Get current surface texture
		WGPUSurfaceTexture surface_texture;
		wgpuSurfaceGetCurrentTexture(demo.surface, &surface_texture);

		switch (surface_texture.status) {
		case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
		case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
			// All good
			break;
		case WGPUSurfaceGetCurrentTextureStatus_Timeout:
		case WGPUSurfaceGetCurrentTextureStatus_Outdated:
		case WGPUSurfaceGetCurrentTextureStatus_Lost: {
			// Skip this frame and reconfigure surface
			if (surface_texture.texture != NULL) {
				wgpuTextureRelease(surface_texture.texture);
			}
			SDL_GetWindowSize(window, &width, &height);
			if (width != 0 && height != 0) {
				demo.config.width = width;
				demo.config.height = height;
				wgpuSurfaceConfigure(demo.surface, &demo.config);
			}
			continue;
		}
		case WGPUSurfaceGetCurrentTextureStatus_Error:
		case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
		case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
		case WGPUSurfaceGetCurrentTextureStatus_Force32:
			// Fatal error
			printf(LOG_PREFIX " get_current_texture status=%#.8x\n",
			       surface_texture.status);
			abort();
		}
		assert(surface_texture.texture);

		// Update uniform buffer with mouse position and resolution
		struct uniforms_data uniforms = {
		    .mouse_x = mouse_x,
		    .mouse_y = mouse_y,
		    .resolution_x = (float)width,
		    .resolution_y = (float)height,
		};
		wgpuQueueWriteBuffer(queue, uniform_buffer, 0, &uniforms, sizeof(uniforms));

		// Create texture view
		WGPUTextureView frame = wgpuTextureCreateView(surface_texture.texture, NULL);
		assert(frame);

		// Create command encoder
		WGPUCommandEncoder command_encoder = wgpuDeviceCreateCommandEncoder(
		    demo.device, &(const WGPUCommandEncoderDescriptor){
		                     .label = {"command_encoder", WGPU_STRLEN},
		                 });
		assert(command_encoder);

		// Begin render pass
		WGPURenderPassEncoder render_pass_encoder =
		    wgpuCommandEncoderBeginRenderPass(
		        command_encoder,
		        &(const WGPURenderPassDescriptor){
		            .label = {"render_pass_encoder", WGPU_STRLEN},
		            .colorAttachmentCount = 1,
		            .colorAttachments =
		                (const WGPURenderPassColorAttachment[]){
		                    (const WGPURenderPassColorAttachment){
		                        .view = frame,
		                        .loadOp = WGPULoadOp_Clear,
		                        .storeOp = WGPUStoreOp_Store,
		                        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
		                        .clearValue =
		                            (const WGPUColor){
		                                .r = 0.0,
		                                .g = 1.0,
		                                .b = 0.0,
		                                .a = 1.0,
		                            },
		                    },
		                },
		        });
		assert(render_pass_encoder);

		// Draw triangle with bind group
		wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
		wgpuRenderPassEncoderSetBindGroup(render_pass_encoder, 0, bind_group, 0, NULL);
		wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
		wgpuRenderPassEncoderEnd(render_pass_encoder);
		wgpuRenderPassEncoderRelease(render_pass_encoder);

		// Finish command buffer
		WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(
		    command_encoder, &(const WGPUCommandBufferDescriptor){
		                         .label = {"command_buffer", WGPU_STRLEN},
		                     });
		assert(command_buffer);

		// Submit and present
		wgpuQueueSubmit(queue, 1, (const WGPUCommandBuffer[]){command_buffer});
		wgpuSurfacePresent(demo.surface);

		// Cleanup frame resources
		wgpuCommandBufferRelease(command_buffer);
		wgpuCommandEncoderRelease(command_encoder);
		wgpuTextureViewRelease(frame);
		wgpuTextureRelease(surface_texture.texture);

		// Calculate and print FPS
		frame_count++;
		Uint64 current_time = SDL_GetTicks();
		Uint64 elapsed = current_time - last_fps_print;

		// Print FPS every second
		if (elapsed >= 1000) {
			float fps = frame_count * 1000.0f / elapsed;
			printf("FPS: %.1f\n", fps);
			frame_count = 0;
			last_fps_print = current_time;
		}
	}

	// Cleanup
	wgpuRenderPipelineRelease(render_pipeline);
	wgpuPipelineLayoutRelease(pipeline_layout);
	wgpuBindGroupRelease(bind_group);
	wgpuBindGroupLayoutRelease(bind_group_layout);
	wgpuBufferRelease(uniform_buffer);
	wgpuShaderModuleRelease(shader_module);
	wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
	wgpuQueueRelease(queue);
	wgpuDeviceRelease(demo.device);
	wgpuAdapterRelease(demo.adapter);
	wgpuSurfaceRelease(demo.surface);
	SDL_DestroyWindow(window);
	wgpuInstanceRelease(demo.instance);
	SDL_Quit();

	return 0;
}
