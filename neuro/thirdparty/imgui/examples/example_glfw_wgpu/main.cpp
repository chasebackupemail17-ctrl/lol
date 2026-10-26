

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_wgpu.h"
#include <stdio.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#else
#include <webgpu/webgpu_glfw.h>
#endif

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static WGPUInstance      wgpu_instance = nullptr;
static WGPUDevice        wgpu_device = nullptr;
static WGPUSurface       wgpu_surface = nullptr;
static WGPUTextureFormat wgpu_preferred_fmt = WGPUTextureFormat_RGBA8Unorm;
static WGPUSwapChain     wgpu_swap_chain = nullptr;
static int               wgpu_swap_chain_width = 1280;
static int               wgpu_swap_chain_height = 720;

static bool InitWGPU(GLFWwindow* window);
static void CreateSwapChain(int width, int height);

static void glfw_error_callback(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

static void wgpu_error_callback(WGPUErrorType error_type, const char* message, void*)
{
    const char* error_type_lbl = "";
    switch (error_type)
    {
    case WGPUErrorType_Validation:  error_type_lbl = "Validation"; break;
    case WGPUErrorType_OutOfMemory: error_type_lbl = "Out of memory"; break;
    case WGPUErrorType_Unknown:     error_type_lbl = "Unknown"; break;
    case WGPUErrorType_DeviceLost:  error_type_lbl = "Device lost"; break;
    default:                        error_type_lbl = "Unknown";
    }
    printf("%s error: %s\n", error_type_lbl, message);
}

int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(wgpu_swap_chain_width, wgpu_swap_chain_height, "Dear ImGui GLFW+WebGPU example", nullptr, nullptr);
    if (window == nullptr)
        return 1;

    if (!InitWGPU(window))
    {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    CreateSwapChain(wgpu_swap_chain_width, wgpu_swap_chain_height);
    glfwShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplWGPU_InitInfo init_info;
    init_info.Device = wgpu_device;
    init_info.NumFramesInFlight = 3;
    init_info.RenderTargetFormat = wgpu_preferred_fmt;
    init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init_info);

#ifndef IMGUI_DISABLE_FILE_FUNCTIONS

#endif

    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

#ifdef __EMSCRIPTEN__

    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {

        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);
        if (width != wgpu_swap_chain_width || height != wgpu_swap_chain_height)
        {
            ImGui_ImplWGPU_InvalidateDeviceObjects();
            CreateSwapChain(width, height);
            ImGui_ImplWGPU_CreateDeviceObjects();
        }

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");

            ImGui::Text("This is some useful text.");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float*)&clear_color);

            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        ImGui::Render();

#ifndef __EMSCRIPTEN__

        wgpuDeviceTick(wgpu_device);
#endif

        WGPURenderPassColorAttachment color_attachments = {};
        color_attachments.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        color_attachments.loadOp = WGPULoadOp_Clear;
        color_attachments.storeOp = WGPUStoreOp_Store;
        color_attachments.clearValue = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        color_attachments.view = wgpuSwapChainGetCurrentTextureView(wgpu_swap_chain);

        WGPURenderPassDescriptor render_pass_desc = {};
        render_pass_desc.colorAttachmentCount = 1;
        render_pass_desc.colorAttachments = &color_attachments;
        render_pass_desc.depthStencilAttachment = nullptr;

        WGPUCommandEncoderDescriptor enc_desc = {};
        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpu_device, &enc_desc);

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
        wgpuRenderPassEncoderEnd(pass);

        WGPUCommandBufferDescriptor cmd_buffer_desc = {};
        WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
        WGPUQueue queue = wgpuDeviceGetQueue(wgpu_device);
        wgpuQueueSubmit(queue, 1, &cmd_buffer);

#ifndef __EMSCRIPTEN__
        wgpuSwapChainPresent(wgpu_swap_chain);
#endif

        wgpuTextureViewRelease(color_attachments.view);
        wgpuRenderPassEncoderRelease(pass);
        wgpuCommandEncoderRelease(encoder);
        wgpuCommandBufferRelease(cmd_buffer);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#ifndef __EMSCRIPTEN__
static WGPUAdapter RequestAdapter(WGPUInstance instance)
{
    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* pUserData)
    {
        if (status == WGPURequestAdapterStatus_Success)
            *(WGPUAdapter*)(pUserData) = adapter;
        else
            printf("Could not get WebGPU adapter: %s\n", message);
};
    WGPUAdapter adapter;
    wgpuInstanceRequestAdapter(instance, nullptr, onAdapterRequestEnded, (void*)&adapter);
    return adapter;
}

static WGPUDevice RequestDevice(WGPUAdapter& adapter)
{
    auto onDeviceRequestEnded = [](WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* pUserData)
    {
        if (status == WGPURequestDeviceStatus_Success)
            *(WGPUDevice*)(pUserData) = device;
        else
            printf("Could not get WebGPU device: %s\n", message);
    };
    WGPUDevice device;
    wgpuAdapterRequestDevice(adapter, nullptr, onDeviceRequestEnded, (void*)&device);
    return device;
}
#endif

static bool InitWGPU(GLFWwindow* window)
{
    wgpu::Instance instance = wgpuCreateInstance(nullptr);

#ifdef __EMSCRIPTEN__
    wgpu_device = emscripten_webgpu_get_device();
    if (!wgpu_device)
        return false;
#else
    WGPUAdapter adapter = RequestAdapter(instance.Get());
    if (!adapter)
        return false;
    wgpu_device = RequestDevice(adapter);
#endif

#ifdef __EMSCRIPTEN__
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
    html_surface_desc.selector = "#canvas";
    wgpu::SurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &html_surface_desc;
    wgpu::Surface surface = instance.CreateSurface(&surface_desc);

    wgpu::Adapter adapter = {};
    wgpu_preferred_fmt = (WGPUTextureFormat)surface.GetPreferredFormat(adapter);
#else
    wgpu::Surface surface = wgpu::glfw::CreateSurfaceForWindow(instance, window);
    if (!surface)
        return false;
    wgpu_preferred_fmt = WGPUTextureFormat_BGRA8Unorm;
#endif

    wgpu_instance = instance.MoveToCHandle();
    wgpu_surface = surface.MoveToCHandle();

    wgpuDeviceSetUncapturedErrorCallback(wgpu_device, wgpu_error_callback, nullptr);

    return true;
}

static void CreateSwapChain(int width, int height)
{
    if (wgpu_swap_chain)
        wgpuSwapChainRelease(wgpu_swap_chain);
    wgpu_swap_chain_width = width;
    wgpu_swap_chain_height = height;
    WGPUSwapChainDescriptor swap_chain_desc = {};
    swap_chain_desc.usage = WGPUTextureUsage_RenderAttachment;
    swap_chain_desc.format = wgpu_preferred_fmt;
    swap_chain_desc.width = width;
    swap_chain_desc.height = height;
    swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
    wgpu_swap_chain = wgpuDeviceCreateSwapChain(wgpu_device, wgpu_surface, &swap_chain_desc);
}
