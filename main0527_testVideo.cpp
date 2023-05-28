#include <GL/glew.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>

#include <iostream>

#include <filesystem>
//#include "camProps.h"
#include "CameraManager.h"
#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"
//#include "sdlManager.h"
#include "utils.h"
#include "AppState.h"
#include "glUtil.h"
//#include "secondaryMonitorManager.h"
// for 로깅
#include <fstream>
#include <string>
#include <ctime>
#include <exception>
//#include <opencv2/highgui/highgui.hpp>
#include <chrono>

namespace fs = std::filesystem;
std::atomic<bool> runningRenderWindow{true};
static bool docking_disabled = false;
void FileBrowser(const char* directory, const char* fileExtension);
void renderWindow(SDL_Window* window, CameraManager& cameraManager, float red, float green, float blue);


void Log(const std::string& text)
{
    std::ofstream logFile;

    // 파일을 열고, 파일 끝에 내용을 추가하는 모드로 설정합니다.
    logFile.open("log.txt", std::ios_base::app);

    // 현재 시간을 가져옵니다.
    std::time_t now = std::time(0);
    std::string strNow = std::ctime(&now);

    // 현재 시간과 메시지를 로그 파일에 쓰기
    logFile << strNow.substr(0, strNow.size() - 1) << " " << text << std::endl;

    // 파일을 닫습니다.
    logFile.close();
}


enum class InputSource {
    Webcam,
    JpgImage,
    Mp4Video
};

// Define the AppState instance
AppState appState;
SDL_Window* window;
SDL_GLContext gl_context;
SDL_Window* window1;
SDL_GLContext gl_context1;
static const ImWchar ranges[] =
{
    0x0020, 0x00FF, // Basic Latin + Latin Supplement
    0x4e00, 0x9FAF, // CJK Ideograms
    0,
};
void drawTexture(SDL_Window* window, SDL_GLContext context, GLuint textureID) {
    // Make this window's context the current one
    SDL_GL_MakeCurrent(window, context);

    // Use the texture
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Draw a quad
    glBegin(GL_QUADS);
    glTexCoord2i(0, 0); glVertex2i(-1, -1);
    glTexCoord2i(0, 1); glVertex2i(-1, 1);
    glTexCoord2i(1, 1); glVertex2i(1, 1);
    glTexCoord2i(1, 0); glVertex2i(1, -1);
    glEnd();

    // Swap buffers
    SDL_GL_SwapWindow(window);
}
int sub_main()
{
    Log("Program started");

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cout << "Error: " << SDL_GetError() << std::endl;
        return -1;
    }
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    // From 2.0.18: Enable native IME.
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");


    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 example", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Initialize GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
        return 1;
    }
    // 
        // nDisplay 모니터 체크
// 디스플레이 개수 파악
    int displays = SDL_GetNumVideoDisplays();
    assert(displays > 1);  // 두 번째 모니터가 있다고 가정

    // 모든 디스플레이의 경계 얻기
    vector<SDL_Rect> displayBounds;
    for (int i = 0; i < displays; i++) {
        displayBounds.push_back(SDL_Rect());
        SDL_GetDisplayBounds(i, &displayBounds.back());
    }
    // 두 번째 윈도우를 두 번째 디스플레이에 만듭니다.
    window1 = SDL_CreateWindow("Second Window",
        displayBounds[1].x, // x position
        displayBounds[1].y, // y position
        //640, // width
        //480, // height
        displayBounds[1].w, // width
        displayBounds[1].h, // height

        SDL_WINDOW_OPENGL);

    if (window1 == NULL) {
        SDL_Log("Could not create window: %s\n", SDL_GetError());
        return 1;
    }


    // 카메라 매니저
    int numberOfCams = 4;
    CameraManager cameraManager;
    for (int i = 0; i < numberOfCams; i++) {
        cameraManager.addCamera(i, 0);
    }
    std::vector<InputSource> inputSources(4, InputSource::Webcam);  // Default to webcam
    // 웹캠 캡처 시작
    cameraManager.startCapture();
    // 프로그램 종료 전에 캡쳐 종료.
    //

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    //ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\Malgun.ttf", 16.0f, NULL, ranges);
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

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts


        // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // renderWindow 함수를 별도의 스레드에서 실행
    std::thread renderThread(renderWindow, window1, std::ref(cameraManager), 0.0f, 128.0f, 0.0f);


    // Main loop
    bool done = false;

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                runningRenderWindow = false;
                done = true;
                if (renderThread.joinable()) {
                    renderThread.join();
                }
            }
                
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
            {
                runningRenderWindow = false;
                done = true;
                if (renderThread.joinable()) {
                    renderThread.join();
                }
            }
                
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F8)
            {
                docking_disabled = !docking_disabled; // 도킹 활성화 토글
            }
        }

        // Start the Dear ImGui frame
        // ImGui 프레임 시작
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        // 도킹 활성화
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
        ImGuiWindowFlags window_flags = 0;
        if (docking_disabled) {
            window_flags |= ImGuiWindowFlags_NoDocking;
        }

        // Create a full-width top menu bar
        // 상단 메뉴바

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) { /* TODO: Insert code to handle this event */ }
                ImGui::EndMenu();
            }

            // Add other menus as needed

            ImGui::EndMainMenuBar();
        }

        // Create a dockable window
        ImGui::SetNextWindowSize(ImVec2(200, 0), ImGuiCond_FirstUseEver); // Set initial size
        if (ImGui::Begin("Control Window", nullptr, ImGuiWindowFlags_NoCollapse))
        {
            // Add buttons and other controls here
            ImGui::Text(u8"Hello, 世界!");
            ImGui::Button("Button 1");
            ImGui::Button("Button 2");

            // ...
            ImGui::End();
        }
        // file browser
        FileBrowser("d:\\iotaflux\\image", ".jpg");

        //ImGui::Begin("Camera Frames");
        //GLuint textureID = cameraManager.getCamera(0).getTexture();
        //if (textureID != 0) {
        //    auto [width, height] = cameraManager.getCameraTextureSize(0);
        //    ImGui::Image((void*)(intptr_t)textureID, ImVec2(width, height));
        //}
        //ImGui::End();


        // Create 2x2 viewer grid
        ImGui::Begin("2x2 Viewer Grid", nullptr, window_flags);
        for (int y = 0; y < 2; ++y)
        {
            for (int x = 0; x < 2; ++x)
            {
                int id = y * 2 + x;
                ImGui::PushID(id);
                ImGui::BeginChild("Viewer", ImVec2(ImGui::GetWindowContentRegionWidth() / 2, (ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()) / 2), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

                // Get the texture from the corresponding camera
                GLuint textureID = cameraManager.getCamera(id).getTexture();
                auto [width, height] = cameraManager.getCameraTextureSize(id);
                ImVec2 textureSize = ImVec2(width, height);

                if (textureID != 0) {
                    ImGuiDisplayTextureMaintainAspectRatio((void*)(intptr_t)textureID, textureSize);
                }

                //if (textureID != 0) {
                //    ImGui::Image((void*)(intptr_t)textureID, textureSize);
                //}
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
                {
                    cameraManager.setSelectedCamera(id);
                    ImGui::OpenPopup("Options");
                }
                // Rest of the viewer code...
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
                {
                    ImGui::OpenPopup("Options");
                }

                if (ImGui::BeginPopup("Options"))
                {
                    char buffer[32];

                    snprintf(buffer, sizeof(buffer), "Option for viewer %d", id);
                    if (ImGui::MenuItem(buffer))
                    {
                        // Implement functionality here
                    }

                    snprintf(buffer, sizeof(buffer), "Another option for viewer %d", id);
                    if (ImGui::MenuItem(buffer))
                    {
                        // Implement functionality here
                    }
                    ImGui::EndPopup();
                }

                ImGui::EndChild();
                ImGui::PopID();

                if (x < 1) ImGui::SameLine();
            }
        }
        ImGui::End();

        // Create 2x2 viewer grid
        //ImGui::Begin("2x2 Viewer Grid", nullptr);
        //for (int y = 0; y < 2; ++y)
        //{
        //    for (int x = 0; x < 2; ++x)
        //    {
        //        int id = y * 2 + x;
        //        ImGui::PushID(id);
        //        ImGui::BeginChild("Viewer", ImVec2(ImGui::GetWindowContentRegionWidth() / 2, (ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()) / 2), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);



        //        // Insert viewer content here...
        //        ImGui::Text("Viewer %d", id); // example content

        //        if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
        //        {
        //            ImGui::OpenPopup("Options");
        //        }

        //        if (ImGui::BeginPopup("Options"))
        //        {
        //            char buffer[32];

        //            snprintf(buffer, sizeof(buffer), "Option for viewer %d", id);
        //            if (ImGui::MenuItem(buffer))
        //            {
        //                // Implement functionality here
        //            }

        //            snprintf(buffer, sizeof(buffer), "Another option for viewer %d", id);
        //            if (ImGui::MenuItem(buffer))
        //            {
        //                // Implement functionality here
        //            }
        //            ImGui::EndPopup();
        //        }

        //        ImGui::EndChild();
        //        ImGui::PopID();

        //        if (x < 1) ImGui::SameLine();
        //    }
        //}
        //ImGui::End();

        // Create 1x3 viewer grid
        ImGui::Begin("13Viewer Grid", nullptr, window_flags);
        for (int x = 0; x < 3; ++x)
        {
            ImGui::PushID(x);
            ImGui::BeginChild("Viewer", ImVec2(ImGui::GetWindowContentRegionWidth() / 3, (ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing())), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Insert viewer content here...
            ImGui::Text("Viewer %d", x); // example content

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
            {
                ImGui::OpenPopup("Options");
            }

            if (ImGui::BeginPopup("Options"))
            {
                char buffer[32];

                snprintf(buffer, sizeof(buffer), "Option for viewer %d", x);
                if (ImGui::MenuItem(buffer))
                {
                    // Implement functionality here
                }

                snprintf(buffer, sizeof(buffer), "Another option for viewer %d", x);
                if (ImGui::MenuItem(buffer))
                {
                    // Implement functionality here
                }
                ImGui::EndPopup();
            }

            ImGui::EndChild();
            ImGui::PopID();

            if (x < 2) ImGui::SameLine();
        }
        ImGui::End();
        // Create 3x1 viewer grid
        ImGui::Begin("31 Viewer Grid", nullptr, window_flags);
        for (int y = 0; y < 3; ++y)
        {
            ImGui::PushID(y);
            ImGui::BeginChild("Viewer", ImVec2(ImGui::GetWindowContentRegionWidth(), (ImGui::GetWindowHeight() - ImGui::GetFrameHeightWithSpacing()) / 3), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Insert viewer content here...
            ImGui::Text("Viewer %d", y); // example content

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
            {
                ImGui::OpenPopup("Options");
            }

            if (ImGui::BeginPopup("Options"))
            {
                char buffer[32];

                snprintf(buffer, sizeof(buffer), "Option for viewer %d", y);
                if (ImGui::MenuItem(buffer))
                {
                    // Implement functionality here
                }

                snprintf(buffer, sizeof(buffer), "Another option for viewer %d", y);
                if (ImGui::MenuItem(buffer))
                {
                    // Implement functionality here
                }
                ImGui::EndPopup();
            }

            ImGui::EndChild();
            ImGui::PopID();
        }

        ImGui::End();

        // Create bottom tab window
        ImGui::Begin("Tab Window", nullptr, window_flags);
        if (ImGui::BeginTabBar("Tabs"))
        {
            if (ImGui::BeginTabItem("Tab 1"))
            {
                // Insert tab content here...
                ImGui::EndTabItem();
            }
            // Add more tabs as needed...

            ImGui::EndTabBar();
        }
        ImGui::End();

        //if (show_demo_window)
        //    ImGui::ShowDemoWindow(&show_demo_window);

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

        // Second window
        //SDL_GL_MakeCurrent(window1, gl_context1); // activate the OpenGL context for the second window
        //GLuint textureID1 = cameraManager.getCamera(0).getTexture();
        //drawTexture(window1, gl_context1, textureID1);
        //SDL_GL_SwapWindow(window1); // swap buffers for the second window
    }

    // Cleanup
        // 웹캠 캡처 종료
    //renderThread.join();

    cameraManager.stopCapture();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;

}
int testGL() {
    // Create a Camera instance with camera index 0 and monitor index 0
    Camera cam(0, 0);

    // Start the camera capture
    cam.startCapture();

    // Loop for a while
    while(true) {
        // Get the current frame
        cv::Mat frame;
        cam.getFrame(frame);
        
        // Display the frame
        if (!frame.empty()) 
            cout << "frame size: " << frame.size << endl; {
			cv::imshow("Camera", frame);
			cv::waitKey(1); // waits for a key event for 1ms
            //cam.createTexture();
		}
        //cv::imshow("Camera", frame);
        //cv::waitKey(1); // waits for a key event for 1ms

        // Create a texture
        
    }

    // Stop the camera capture
    cam.stopCapture();

    return 0;
}
int main(int , char**) {
    try {
        // Program start log
        Log("Program started");

        // ... program code goes here ...
        sub_main();
        //testGL();
        // Program end log
        Log("Program ended");
    }
    catch (const std::exception& e) {
        // Log when an exception occurs
        Log(std::string("Error occurred: ") + e.what());
    }
    return 0;
}

//ImTextureID createBlackTexture() {
//    // Generate a texture object
//    GLuint texture;
//    glGenTextures(1, &texture);
//    glBindTexture(GL_TEXTURE_2D, texture);
//
//    // Black pixel
//    unsigned char black[3] = { 0, 0, 0 };
//
//    // Set the texture's data
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, black);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    return reinterpret_cast<void*>(static_cast<intptr_t>(texture));
//}

//ImTextureID loadTexture(const std::string& filename) {
//    int width, height, channels;
//    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
//    if (data == nullptr) {
//        std::cerr << "Failed to load image: " << filename << std::endl;
//        return nullptr;
//    }
//
//    GLuint texture;
//    glGenTextures(1, &texture);
//    glBindTexture(GL_TEXTURE_2D, texture);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//    glBindTexture(GL_TEXTURE_2D, 0);
//
//    stbi_image_free(data);
//
//    return reinterpret_cast<void*>(static_cast<intptr_t>(texture));
//}


void renderWindow(SDL_Window* window, CameraManager& cameraManager, float red, float green, float blue)
{
    SDL_GLContext context = SDL_GL_CreateContext(window);
    glewExperimental = GL_TRUE;
    glewInit();


    GLuint webcamTexture;
    glGenTextures(1, &webcamTexture);

    GLuint shaderProgram2 = initializeShader();
    // Set up the OpenGL quad for rendering the texture
    GLuint vao, vbo;
    GLfloat quadVertices[] = {
        // Positions      // Texture coordinates
        -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
         1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f, 0.0f, 1.0f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));



    bool running = true;
    while (running && runningRenderWindow)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
        }
        int camIndex = cameraManager.getSelectedCameraIndex();
        if (camIndex > 4) {
			camIndex = 4;
		}
        Camera& camera = cameraManager.getCamera(camIndex);
        cv::Mat frame;
        camera.getFrame(frame);
        updateTexture(frame, webcamTexture);
        glUseProgram(shaderProgram2);
        glBindTexture(GL_TEXTURE_2D, webcamTexture);
        //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frame.cols, frame.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.data);

        
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);


        //glClearColor(red, green, blue, 1.0f);
        //glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
        // 대기 시간을 추가합니다. 이 경우에는 16밀리초 동안 대기하게 됩니다.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SDL_GL_DeleteContext(context);
}
