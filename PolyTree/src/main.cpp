#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ImGuiFileBrowser.h"

#include "AtariObj.h"
#include "Shader.h"

#define OPEN_FILE "Open File"

void fbSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main(int argc, char** argv)
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* glsl_version = "#version 130";
    char textBuffer[1024];
    textBuffer[0] = '\0';

    AtariObj* obj = NULL;

    GLFWwindow* window = glfwCreateWindow(800, 600, "PolyTree", NULL, NULL);

    if (!window)
    {
        printf("Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        printf("Failed to initialise GLAD\n");
        return -1;
    }

    glViewport(0, 0, 800, 600);
    glfwSetFramebufferSizeCallback(window, fbSizeCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    bool showFileDialog = false;
    imgui_addons::ImGuiFileBrowser fileDialog; // As a class member or globally

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
 
    Shader* s = new Shader("vertex.glsl", "frag.glsl");

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open...", NULL))
                {
                    showFileDialog = true;
                }

                if (ImGui::MenuItem("Quit", NULL))
                {
					glfwSetWindowShouldClose(window, true);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        if (showFileDialog)
        {
            ImGui::OpenPopup(OPEN_FILE);
        }

        if (fileDialog.showFileDialog(OPEN_FILE, imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(100, 100), ".obj,.OBJ"))
        {
            std::cout << fileDialog.selected_fn << std::endl;      // The name of the selected file or directory in case of Select Directory dialog mode
            std::cout << fileDialog.selected_path << std::endl;    // The absolute path to the selected file
            strcpy(textBuffer, fileDialog.selected_path.c_str());
            showFileDialog = false;

            if (obj)
            {
                delete obj;
            }

            obj = new AtariObj(textBuffer);
        }

        ImGui::Begin("Object Info", NULL);
        ImGui::SetWindowSize(ImVec2(400.0f, 100.0f));
        ImGui::Text("Current File: %s\n", textBuffer);

        if (obj)
        {
            ImGui::Text("Vert Count: %i\nFace Count: %i\n", obj->o.vertCount, obj->o.faceCount);
        }

        ImGui::End();

        // Rendering
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        s->Use();

        if (obj)
        {
            obj->Render();
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (obj)
    {
        delete obj;
    }

    delete s;

    glfwDestroyWindow(window);
    glfwTerminate();

	return 0;
}
