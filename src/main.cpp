#include "raylib.h"
#include "imgui.h"
#include "../include/rlImGui.h"
#include "../include/imgui_impl_raylib.h"
#include "../include/imgui.h"
#include <iostream>
#include "vector"
#include "memory"

struct Config
{
    // screen configs
    int screenWidth  = 1200;
    int screenHeight = 800;
    const char* windowTitle = "Indium - Game Engine";

    // target configs
    int  targetFps = 165;
    bool showFps   = false;
};

struct CircleDefaults
{
    Color color    = WHITE;
    float radius   = 50.0f;
    int   centerX  = 600 - radius / 2;
    int   centerY  = 400 - radius / 2;
};

struct RectangleDefaults
{
    Color color  = WHITE;
    int   height = 200;
    int   width  = 100;
    int   posX   = 600 - height / 2;
    int   posY   = 400 - width / 2;
};

struct Circle
{
    Vector2 position;
    float   radius;
    Color   color;
};

struct RectShape
{
    Vector2 position;
    Vector2 scale;
    Color   color;
};

struct BillBoard
{
    Texture2D texture;
    Vector3   position;
    Camera    camera;
    Color     tint;
    float     scale;
};

struct Cube
{
    Vector3 position;
    Vector3 size;
    Color   color;
};

class ManageCircles
{
    private:
        CircleDefaults defaults;
        Config         config;
    public:
        Circle CreateCircle()
        {
            return {
                {(float)GetRandomValue(400,config.screenWidth - 200),
                (float)GetRandomValue(400,config.screenWidth - 200)},
                defaults.radius,
                defaults.color
            };
        }

        ~ManageCircles() = default;
};

class ManageRectangles
{
    private:
        RectangleDefaults defaults;
        Config            config;
    public:
        RectShape CreateRectangle()
        {
            return {
                {(float)GetRandomValue(400,config.screenWidth - 200),
                (float)GetRandomValue(400,config.screenWidth - 200)},
                {(float)defaults.height,(float)defaults.width},
                defaults.color
            };
        }

        ~ManageRectangles() = default;
};

int main()
{
    Config config;
    std::vector<std::unique_ptr<RectShape>> rectangles;
    std::vector<std::unique_ptr<BillBoard>> billboards;
    std::vector<std::unique_ptr<Circle>>    circles;
    std::vector<std::unique_ptr<Cube>>      cubes;

    ManageCircles    manageCircles;
    ManageRectangles manageRectangles;
    //
    InitWindow(config.screenWidth,config.screenHeight,config.windowTitle);
    SetTargetFPS(config.targetFps);
    rlImGuiSetup(true);

    while (!WindowShouldClose())
    {
        Vector2 mouse = GetMousePosition();

        
        BeginDrawing();
            ClearBackground(BLACK);
            if (config.showFps) DrawFPS(10,10);
            rlImGuiBegin();
                ImGui::Begin("Indium control panel");
                    ImGui::Text("Hello, This is our first game engine");
                    if (ImGui::Button("Draw circle",(ImVec2){100,20}))
                    {
                        Circle circle = manageCircles.CreateCircle();
                        circles.push_back(std::make_unique<Circle>(circle));
                    }
                    if(ImGui::Button("Show Fps",(ImVec2){100,20}))
                    {
                        config.showFps = !config.showFps;
                    }
                    if(ImGui::Button("Draw Rectangle",(ImVec2){100,20}))
                    {
                        RectShape rectangle = manageRectangles.CreateRectangle();
                        rectangles.push_back(std::make_unique<RectShape>(rectangle)); 
                    }
                ImGui::End();
            rlImGuiEnd();

        if (circles.size() > 0)
        {
            for(auto& c: circles)
            {
                DrawCircleV(c->position,c->radius,c->color);
            }
        }

        if (cubes.size() > 0)
        {
            for(auto& c: cubes)
            {
                DrawCubeV(c->position,c->size,c->color);
            }
        }

        if(rectangles.size() > 0)
        {
            for(auto& r: rectangles)
            {
                DrawRectangle(r->position.x,r->position.y,r->scale.x,r->scale.y,r->color);
            }
        }
        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
