#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "ImGuiFileBrowser.h"

#include "AtariObj.h"
#include "Shader.h"

#define OPEN_FILE "Open File"
#define SAVE_FILE "Save File"

void fbSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void imGuiObjectTree(ObjNode* pNode)
{
    if (pNode->pPart)
    {
        ImGui::Text("Face Count: %i", pNode->pPart->faceCount);
    }
    else
    {
        if (ImGui::TreeNode("L"))
        {
            imGuiObjectTree(pNode->pLeft);
			ImGui::TreePop();
        }

        if (ImGui::TreeNode("R"))
        {
			imGuiObjectTree(pNode->pRight);
			ImGui::TreePop();
        }
    }
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
    float rot = 0.0f, rotSpeed = 0.0f;
    glm::mat4 projection, view;


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

    view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0, 0.0, -5.0));

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

    int showFileDialog = 0;
    bool showSaveFileDialog = false;
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
                if (ImGui::MenuItem("Open Obj...", NULL))
                {
                    showFileDialog = 1;
                }

                if (ImGui::MenuItem("Open LTO...", NULL))
                {
                    showFileDialog = 2;
                }

                if (ImGui::MenuItem("Export...", NULL, false, (obj && !showFileDialog)))
                {
                    showSaveFileDialog = true;
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
        else if (showSaveFileDialog)
        {
            ImGui::OpenPopup(SAVE_FILE);
        }

        if (fileDialog.showFileDialog(OPEN_FILE, imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(100, 100), (showFileDialog == 1 ? ".obj,.OBJ" : ".lto,.LTO")))
        {
            std::cout << fileDialog.selected_fn << std::endl;      // The name of the selected file or directory in case of Select Directory dialog mode
            std::cout << fileDialog.selected_path << std::endl;    // The absolute path to the selected file
            strcpy(textBuffer, fileDialog.selected_path.c_str());
            showFileDialog = 0;

            if (obj)
            {
                delete obj;
            }

            obj = new AtariObj(textBuffer);
        }
        else if (fileDialog.showFileDialog(SAVE_FILE, imgui_addons::ImGuiFileBrowser::DialogMode::SAVE, ImVec2(100, 100), ".lto,.LTO"))
        {
            strcpy(textBuffer, fileDialog.selected_path.c_str());
            showSaveFileDialog = false;

            if (obj)
            {
                obj->WriteTree(textBuffer);
            }
        }

        ImGui::Begin("Object Info", NULL);
        ImGui::Text("Current File: %s\n", textBuffer);

        if (obj)
        {
            ImGui::Text("Vert Count: %i\nFace Count: %i\n", obj->o.vertCount, obj->o.faceCount);
            ImGui::SliderFloat("Y Rotation", &rotSpeed, -.1f, .1f);
            imGuiObjectTree(obj->o.pRootNode);
        }

        // ImGui::Text("Shader output:\n%s", s->errorLog);

        ImGui::End();

        // Rendering
        ImGui::Render();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);

        projection = glm::perspective(glm::radians(45.0f), (float)display_w / (float)display_h, 0.1f, 100.0f);

        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        s->Use();

        if (obj)
        {
            glm::mat4 trans = glm::mat4(1.0f);
            trans = glm::rotate(trans, glm::radians(30.0f), glm::vec3(1.0, 0.0, 0.0));
            trans = glm::rotate(trans, glm::radians(rot), glm::vec3(0.0, 1.0, 0.0));
            trans = glm::scale(trans, glm::vec3(0.5, 0.5, 0.5));

            rot += rotSpeed;
            if (rot >= 360.0f)
            {
                rot -= 360.0f;
            }
            else if (rot <= 0.0f)
            {
                rot += 360.0f;
            }

            unsigned int transformLoc = glGetUniformLocation(s->programId, "transform");
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

            unsigned int cameraLoc = glGetUniformLocation(s->programId, "camera");
            glUniformMatrix4fv(cameraLoc, 1, GL_FALSE, glm::value_ptr(view));

            unsigned int projLoc = glGetUniformLocation(s->programId, "projection");
            glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

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
