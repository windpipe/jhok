// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <vector>
#include <thread>
#include <mutex>
#include <filesystem>
#include <opencv2/opencv.hpp>

#include "MyApp.h"

#include <nvEncodeAPI.h>
#include "NvEncoder/NvEncoderCuda.h"
#include "NvEncoder/NvEncoderCLIOptions.h"
#include <cuda_runtime.h>
//#include "NvEncoder/Logger.h"

#include <iostream>
#include <chrono>
#include <ctime>

//enum class FileType {
//    Video,
//    Image
//};
int channel1 = 1;
int channel2 = 2;
//using namespace MyApp;
//std::string videoFilename1 = generateFilename(channel1, FileType::Video);
//std::string videoFilename2 = generateFilename(channel2, FileType::Video);
//std::string imageFilename1 = generateFilename(channel1, FileType::Image);
//std::string imageFilename2 = generateFilename(channel2, FileType::Image);
//
//std::cout << "Video filename for channel 1: " << videoFilename1 << std::endl;
//std::cout << "Video filename for channel 2: " << videoFilename2 << std::endl;
//std::cout << "Image filename for channel 1: " << imageFilename1 << std::endl;
//std::cout << "Image filename for channel 2: " << imageFilename2 << std::endl;

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.

std::atomic<bool> capture_running;
simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

// Set the expiration date (YYYY, MM, DD)
constexpr int expiration_year = 2023;
constexpr int expiration_month = 8; // May
constexpr int expiration_day = 15;

bool isExpired() {
    using namespace std::chrono;

    // Get the current system time
    system_clock::time_point now = system_clock::now();
    std::time_t now_c = system_clock::to_time_t(now);

    // Get the expiration date as a time_t
    std::tm expiration_tm = { 0 };
    expiration_tm.tm_year = expiration_year - 1900; // Years since 1900
    expiration_tm.tm_mon = expiration_month - 1; // Months since January [0-11]
    expiration_tm.tm_mday = expiration_day;
    std::time_t expiration_c = mktime(&expiration_tm);

    // Compare the current time and the expiration date
    return difftime(now_c, expiration_c) >= 0;
}


void capture_frames(cv::VideoCapture& cap, cv::Mat& frame, std::mutex& frame_mutex) {
    cv::Mat tmp_frame;
    while (capture_running) {
        cap >> tmp_frame;

        if (tmp_frame.empty()) {
            break;
        }

        {
            std::unique_lock<std::mutex> lock(frame_mutex);
            frame = tmp_frame.clone();
        }
    }
}
void capture_frames2(cv::VideoCapture& cap, cv::Mat& frame, std::mutex& frame_mutex, CameraProperties& cam_props) {
    // Set camera properties
    cap.set(cv::CAP_PROP_FRAME_WIDTH, cam_props.width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, cam_props.height);
    cap.set(cv::CAP_PROP_FPS, cam_props.fps);

    cv::Mat tmp_frame;
    while (capture_running) {
        cap >> tmp_frame;

        if (tmp_frame.empty()) {
            break;
        }

        {
            std::unique_lock<std::mutex> lock(frame_mutex);
            frame = tmp_frame.clone();
        }
    }
}

GLuint matToTexture(const cv::Mat& mat) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mat.cols, mat.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, mat.data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Remember to delete the texture with glDeleteTextures(1, &texture) when it's no longer needed.
    return texture;
}
void ImGuiDisplayTextureMaintainAspectRatio(ImTextureID texture_id, const ImVec2& texture_size) {
    ImVec2 window_size = ImGui::GetContentRegionAvail();

    float aspect_ratio = texture_size.x / texture_size.y;
    ImVec2 scaled_size = ImVec2(window_size.x, window_size.x / aspect_ratio);

    if (scaled_size.y > window_size.y) {
        scaled_size = ImVec2(window_size.y * aspect_ratio, window_size.y);
    }

    ImVec2 cursor_pos = ImGui::GetCursorPos();
    ImVec2 image_pos = ImVec2(cursor_pos.x + (window_size.x - scaled_size.x) * 0.5f, cursor_pos.y + (window_size.y - scaled_size.y) * 0.5f);

    ImGui::SetCursorPos(image_pos);
    ImGui::Image(texture_id, scaled_size);
}


int nDisplay = 1;
int nWebcam = 4; // 1,2,4.

std::mutex frame_mutex1, frame_mutex2;
cv::Mat frame1, frame2;
GLuint vTexture1 = 0;
GLuint vTexture2 = 0;

ImTextureID texture1_id;  // OpenGL texture ID of texture 1
ImTextureID texture2_id;  // OpenGL texture ID of texture 2

bool recordWebcam1 = false;
bool recordWebcam2 = false;

bool prevRecordWebcam1 = false;
bool prevRecordWebcam2 = false;
bool capturePicture1 = false;
bool capturePicture2 = false;

//std::ofstream outputFile1("output_video1.h265", std::ios::binary);
std::unique_ptr<std::ofstream> outputFile1;
// Open output file for writing
//std::ofstream outputFile2("output_video2.h265", std::ios::binary);
std::unique_ptr<std::ofstream> outputFile2;
void initCUDA(int width, int height, int fps);
// Main code
int main(int, char**)
{
    if (isExpired()) {
        std::cout << "The program has expired." << std::endl;
        return 1;
    }
    // at first, check serial number
    //std::string motherboardSerialNumber = MyApp::getMotherboardSerialNumber();
    //std::cout << "Motherboard Serial Number: " << motherboardSerialNumber << std::endl;

    // start program
    int nWidth = 3840, nHeight = 2160;
    NvEncoderInitParam encodeCLIOptions;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    //initCUDA( 3840, 2160, 60);
            // Initialize CUDA
    int nDevice = 0;
    CUdevice cuDevice = 0;
    CUcontext cuContext = NULL;
    ck(cuInit(0));
    ck(cuDeviceGet(&cuDevice, nDevice));
    ck(cuCtxCreate(&cuContext, 0, cuDevice));

    // Initialize NVENC
    NvEncoderCuda enc1(cuContext, nWidth, nHeight, eFormat);
    NvEncoderCuda enc2(cuContext, nWidth, nHeight, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc1.CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_HEVC_GUID, NV_ENC_PRESET_HQ_GUID);
    //NV_ENC_PRESET_P7_GUID); // NV_ENC_PRESET_LOW_LATENCY_HP_GUID);
// Set encoding parameters manually
    initializeParams.encodeWidth = nWidth;
    initializeParams.encodeHeight = nHeight;
    initializeParams.darWidth = nWidth;
    initializeParams.darHeight = nHeight;
    initializeParams.frameRateNum = 30;// 30;
    initializeParams.frameRateDen = 1;
    initializeParams.encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 0; // 8-bit encoding
    initializeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;

    enc1.CreateEncoder(&initializeParams);
    enc2.CreateEncoder(&initializeParams);








    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }
    // nDisplay 모니터 체크
    nDisplay = SDL_GetNumVideoDisplays();
    // Get the current display mode for the primary monitor (0)
    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
        std::cerr << "Error getting display mode: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    int monitorWidth = displayMode.w;
    int monitorHeight = displayMode.h;

    std::cout << "Primary monitor width: " << monitorWidth << std::endl;
    std::cout << "Primary monitor height: " << monitorHeight << std::endl;
    std::vector<SDL_DisplayMode> displayModes(nDisplay);

    for (int i = 0; i < nDisplay; i++) {
        if (SDL_GetCurrentDisplayMode(i, &displayModes[i]) != 0) {
            std::cerr << "Error getting display mode: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return 1;
        }
        std::cout << "Monitor " << i << " width: " << displayModes[i].w << std::endl;
        std::cout << "Monitor " << i << " height: " << displayModes[i].h << std::endl;
    }







    // Decide GL+GLSL versions

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);


    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL  | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED); // | SDL_WINDOW_RESIZABLE
    SDL_Window* window = SDL_CreateWindow("iotaFlux-dx for Test Only", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    MyApp::Colors();
    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    ;;;;
    cv::VideoCapture cap1(0);
    if (!cap1.isOpened()) {
        std::cerr << "Error opening camera!" << std::endl;
        return -1;
    }
    cv::VideoCapture cap2(1);
    if (!cap2.isOpened()) {
        std::cerr << "Error opening camera2!" << std::endl;
        return -1;
    }

    cap1.set(cv::CAP_PROP_FRAME_WIDTH, 3840);
    cap1.set(cv::CAP_PROP_FRAME_HEIGHT, 2160);
    cap1.set(cv::CAP_PROP_FPS, 30);
    cap2.set(cv::CAP_PROP_FRAME_WIDTH, 3840);
    cap2.set(cv::CAP_PROP_FRAME_HEIGHT, 2160);
    cap2.set(cv::CAP_PROP_FPS, 30);
    // Start the capture threads
    capture_running = true;
    std::thread capture_thread1(capture_frames, std::ref(cap1), std::ref(frame1), std::ref(frame_mutex1));
    std::thread capture_thread2(capture_frames, std::ref(cap2), std::ref(frame2), std::ref(frame_mutex2));
    ;;;;

    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = NULL;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!done)
    {

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            else if ( event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3 )
            {
                recordWebcam1 = !recordWebcam1;

                if (recordWebcam1) {
                    // Start recording, open a new output file with a new file name
                    std::string newFilename = MyApp::generateFilename(1, FileType::Video);
                    std::filesystem::path filePath(newFilename);

                    // Create the directory if it doesn't exist
                    std::filesystem::create_directories(filePath.parent_path());
                    outputFile1 = std::make_unique<std::ofstream>(newFilename, std::ios::binary);
                }
                else {
                    // Stop recording, close the current output file
                    outputFile1->close();
                    outputFile1.reset();
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4)
            {
                recordWebcam2 = !recordWebcam2;

                if (recordWebcam2) {
                    // Start recording, open a new output file with a new file name
                    std::string newFilename = MyApp::generateFilename(2, FileType::Video);
                    std::filesystem::path filePath(newFilename);

                    // Create the directory if it doesn't exist
                    std::filesystem::create_directories(filePath.parent_path());
                    outputFile2 = std::make_unique<std::ofstream>(newFilename, std::ios::binary);
                }
                else {
                    // Stop recording, close the current output file
                    outputFile2->close();
                    outputFile2.reset();
                
                }
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F7)
            {
                capturePicture1 = true;
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F8)
            {
                capturePicture2 = true;
            }
        }

        {
            std::unique_lock<std::mutex> lock1(frame_mutex1);
            if (!frame1.empty()) {
                if (vTexture1 != 0)
                {
                    glDeleteTextures(1, &vTexture1);
                }
                if (capturePicture1) {
                    std::string filename = MyApp::generateFilename(1, FileType::Image);
                    std::filesystem::path filePath(filename);

                    // Create the directory if it doesn't exist
                    std::filesystem::create_directories(filePath.parent_path());
                    cv::imwrite(filename, frame1);
                    capturePicture1 = false;
                }

                vTexture1 = matToTexture(frame1);
                texture1_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(vTexture1));



                if (recordWebcam1) {
                    cv::Mat nv12Frame(frame1.rows + frame1.rows / 2, frame1.cols, CV_8UC1);
                    cv::cvtColor(frame1, nv12Frame, cv::COLOR_BGR2YUV_I420);
                    const NvEncInputFrame* encoderInputFrame1 = enc1.GetNextInputFrame();
                    NvEncoderCuda::CopyToDeviceFrame(cuContext, nv12Frame.data, 0, (CUdeviceptr)encoderInputFrame1->inputPtr,
                        (int)encoderInputFrame1->pitch,
                        enc1.GetEncodeWidth(),
                        enc1.GetEncodeHeight(),
                        CU_MEMORYTYPE_HOST,
                        encoderInputFrame1->bufferFormat,
                        encoderInputFrame1->chromaOffsets,
                        encoderInputFrame1->numChromaPlanes);

                    // Encode the frame
                    enc1.EncodeFrame(vPacket1);

                    // Write the packet to file
                                // Process encoded packets (save to file, stream, etc.)
                    for (std::vector<uint8_t>& packet : vPacket1) {
                        outputFile1->write(reinterpret_cast<char*>(packet.data()), packet.size());
                    }

                }
                //if (prevRecordWebcam1 && !recordWebcam1) {
                //    // Flush the encoder
                //    enc1.EndEncode(vPacket1);
                //    // Process encoded packets (save to file, stream, etc.)
                //    for (std::vector<uint8_t>& packet : vPacket1) {
                //        outputFile1->write(reinterpret_cast<char*>(packet.data()), packet.size());
                //    }
                //    vPacket1.clear();
                //   
                //    outputFile1->close();
                //}

            }

        }

        {
            std::unique_lock<std::mutex> lock2(frame_mutex2);
            if (!frame2.empty()) {
                if (vTexture2 != 0)
                {
                    glDeleteTextures(1, &vTexture2);
                }
                vTexture2 = matToTexture(frame2);
                texture2_id = reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(vTexture2));

                if (capturePicture2) {
                    
                    std::string filename = MyApp::generateFilename(2, FileType::Image);
                    std::filesystem::path filePath(filename);

                    // Create the directory if it doesn't exist
                    std::filesystem::create_directories(filePath.parent_path());
                    cv::imwrite(filename, frame2);
                    capturePicture2 = false;
                }
                if (recordWebcam2) {
                    cv::Mat nv12Frame(frame2.rows + frame2.rows / 2, frame2.cols, CV_8UC1);
                    cv::cvtColor(frame2, nv12Frame, cv::COLOR_BGR2YUV_I420);
                    const NvEncInputFrame* encoderInputFrame2 = enc2.GetNextInputFrame();
                    NvEncoderCuda::CopyToDeviceFrame(cuContext, nv12Frame.data, 0, (CUdeviceptr)encoderInputFrame2->inputPtr,
                        (int)encoderInputFrame2->pitch,
                        enc2.GetEncodeWidth(),
                        enc2.GetEncodeHeight(),
                        CU_MEMORYTYPE_HOST,
                        encoderInputFrame2->bufferFormat,
                        encoderInputFrame2->chromaOffsets,
                        encoderInputFrame2->numChromaPlanes);

                    // Encode the frame
                    enc2.EncodeFrame(vPacket2);

                    // Write the packet to file
                                // Process encoded packets (save to file, stream, etc.)
                    for (std::vector<uint8_t>& packet : vPacket2) {
                        outputFile2->write(reinterpret_cast<char*>(packet.data()), packet.size());
                    }

                }
                //if (prevRecordWebcam2 && !recordWebcam2) {
                //    // Flush the encoder
                //    enc2.EndEncode(vPacket2);
                //    // Process encoded packets (save to file, stream, etc.)
                //    //for (std::vector<uint8_t>& packet : vPacket2) {
                //    //    outputFile2->write(reinterpret_cast<char*>(packet.data()), packet.size());
                //    //}
                //    vPacket2.clear();

                //    outputFile2->close();
                //}
            }
        }
        prevRecordWebcam1 = recordWebcam1;
        prevRecordWebcam2 = recordWebcam2;




        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //MyApp::RenderTest();
        //MyApp::RenderUI();
        MyApp::ShowMyWindow();


        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }
        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif
    if ( recordWebcam1) {
        // Flush the encoder
        enc1.EndEncode(vPacket1);
        // Process encoded packets (save to file, stream, etc.)
        for (std::vector<uint8_t>& packet : vPacket1) {
            outputFile1->write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
        vPacket1.clear();

        outputFile1->close();
    }
    if (recordWebcam2) {
        // Flush the encoder
        enc1.EndEncode(vPacket2);
        // Process encoded packets (save to file, stream, etc.)
        for (std::vector<uint8_t>& packet : vPacket2) {
            outputFile2->write(reinterpret_cast<char*>(packet.data()), packet.size());
        }
        vPacket2.clear();

        outputFile2->close();
    }



    std::cout << "quitting main loop" << std::endl;
    // clear encoders.
    enc1.DestroyEncoder();
    enc2.DestroyEncoder();

    capture_running = false;
    capture_thread1.join();
    capture_thread2.join();
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}



void initCUDA(int width, int height, int fps)
{
    // Initialize CUDA
    int nDevice = 0;
    CUdevice cuDevice = 0;
    CUcontext cuContext = NULL;
    NV_ENC_BUFFER_FORMAT eFormat = NV_ENC_BUFFER_FORMAT_IYUV;
    ck(cuInit(0));
    ck(cuDeviceGet(&cuDevice, nDevice));
    ck(cuCtxCreate(&cuContext, 0, cuDevice));

    // Initialize NVENC Encoder
    // Initialize NVENC Encoder
    NvEncoderCuda enc(cuContext, width, height, eFormat);

    NV_ENC_INITIALIZE_PARAMS initializeParams = { NV_ENC_INITIALIZE_PARAMS_VER };
    NV_ENC_CONFIG encodeConfig = { NV_ENC_CONFIG_VER };
    initializeParams.encodeConfig = &encodeConfig;
    enc.CreateDefaultEncoderParams(&initializeParams, NV_ENC_CODEC_HEVC_GUID, NV_ENC_PRESET_HQ_GUID);
    //NV_ENC_PRESET_P7_GUID); // NV_ENC_PRESET_LOW_LATENCY_HP_GUID);
// Set encoding parameters manually
    initializeParams.encodeWidth = width;
    initializeParams.encodeHeight = height;
    initializeParams.darWidth = width;
    initializeParams.darHeight = height;
    initializeParams.frameRateNum = fps;// 30;
    initializeParams.frameRateDen = 1;
    initializeParams.encodeConfig->encodeCodecConfig.hevcConfig.pixelBitDepthMinus8 = 0; // 8-bit encoding
    initializeParams.encodeConfig->rcParams.rateControlMode = NV_ENC_PARAMS_RC_CONSTQP;

    enc.CreateEncoder(&initializeParams);




}
