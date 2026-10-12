#include "gui.hpp"
#include "network/socket.hpp"
#include "network/packet.hpp"
#include "network/network.hpp"
#include "network/server.hpp"
#include "config.hpp"
#include "log.hpp"
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

Gui gGui{};

Gui::Gui() {}
Gui::~Gui() {
    stop();
}

void Gui::start() {
    running = true;
    guiThread = std::thread(&Gui::run, this);
}

void Gui::stop() {
    if (running) {
        running = false;
        if (guiThread.joinable()) {
            guiThread.join();
        }
    }
}

void Gui::run() {
    if (!glfwInit()) {
        Logging::log("GUI", "Failed to initialize GLFW");
        return;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    GLFWwindow *window = glfwCreateWindow(800, 550, "Mirage Panel", nullptr, nullptr);

    glfwSetWindowCloseCallback(window, [](GLFWwindow *win) {
        //glfwSetWindowShouldClose(win, GLFW_FALSE);
        gServerRunning.store(false);
    });

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    char chatInputBuf[256] = "";

    Logging::log("GUI", "Mirage Panel running");

    while (running && !glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration | 
                                       ImGuiWindowFlags_NoMove | 
                                       ImGuiWindowFlags_NoSavedSettings | 
                                       ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("Mirage Panel", nullptr, windowFlags);

        if (ImGui::BeginTabBar("PanelTabs")) {
            if (ImGui::BeginTabItem("Status")) {
                ImGui::Spacing();

                float cardWidth = (ImGui::GetContentRegionAvail().x - 20.0f) / 3.0f;
                
                ImGui::BeginChild("GameBox", ImVec2(cardWidth, 65), true);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Game");
                ImGui::Text("%s", gServerConfig.game.c_str());
                ImGui::EndChild();

                ImGui::SameLine();
                ImGui::BeginChild("VersionBox", ImVec2(cardWidth, 65), true);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Version");
                ImGui::Text("%s", gServerConfig.version.c_str());
                ImGui::EndChild();

                ImGui::SameLine();
                int onlineCount = 0;
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (gNetworkPlayers[i].connected) onlineCount++;
                }
                ImGui::BeginChild("PlayersBox", ImVec2(cardWidth, 65), true);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Players Online");
                ImGui::Text("%d / %d", onlineCount, gServerConfig.maxPlayers);
                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::BeginTable("PlayersTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Id", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Course", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                    ImGui::TableHeadersRow();

                    for (int i = 0; i < MAX_PLAYERS; i++) {
                        const auto &np = gNetworkPlayers[i];
                        if (np.connected) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", i);
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", np.name.c_str());
                            ImGui::TableSetColumnIndex(2); ImGui::Text("%zu", gNetworkPlayerCoopNetIds[gNetworkPlayerPeerIds[i]]);
                            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", np.currCourseNum);
                            ImGui::TableSetColumnIndex(4); ImGui::Text("%d", np.currLevelNum);
                        }
                    }
                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Chat")) {
                ImGui::Spacing();

                float footerHeight = 45.0f;
                if (ImGui::BeginChild("ChatScrollRegion", ImVec2(0, -footerHeight), true, ImGuiWindowFlags_HorizontalScrollbar)) {
                    std::lock_guard<std::mutex> lock(chatMutex);
                    if (chatLog.empty()) {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Chat messages will appear here");
                    } else {
                        for (const auto &msg : chatLog) {
                            ImGui::TextUnformatted(msg.c_str());
                        }
                    }
                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                        ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::EndChild();
                }

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 75.0f);
                bool enterPressed = ImGui::InputText("##ChatInput", chatInputBuf, IM_ARRAYSIZE(chatInputBuf), ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();

                if (ImGui::Button("Send", ImVec2(70, 0)) || enterPressed) {
                    std::string msg(chatInputBuf);
                    if (!msg.empty()) {
                        std::lock_guard<std::mutex> lock(chatMutex);
                        chatLog.push_back("You: " + msg);

                        auto outPkt = CoopPacket::createOutgoing(0, {0}, 0, PACKET_CHAT, true, PLMT_NONE);
                        Logging::log("SERVER", "Sending message: {}", msg);
                        outPkt.write<uint8_t>(0);
                        outPkt.write<uint16_t>(msg.length());
                        outPkt.write<std::string>(msg, msg.length());
                        outPkt.sendToAll();

                        chatInputBuf[0] = '\0';
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        int displayW, displayH;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}