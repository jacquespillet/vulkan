#include "src/App.h"
#include "src/ImguiHelper.h"

vulkanApp App;

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    App.MouseMove((float)xpos, (float)ypos);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    App.MouseAction(button, action, mods);
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    App.Scroll((float)yoffset);
}


void WindowSizeCallback(GLFWwindow* Window, int Width, int Height)
{
    App.Resize(Width, Height);
}

void KeyCallback(GLFWwindow* Window, int Key, int Scancode, int Action, int Mods)
{
    App.KeyEvent(Key, Action);
}

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void WindowSizeCallback(GLFWwindow* Window, int Width, int Height);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

int main(int, char**)
{
    int Width = 1024;
    int Height = 1024;

    GLFWwindow *Window;
    int rc = glfwInit();
    assert(rc);
    glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
    
    Window = glfwCreateWindow(Width, Height, "Gralib", 0, 0);
    assert(Window);

    
    glfwSetCursorPosCallback(Window, CursorPositionCallback);
    glfwSetMouseButtonCallback(Window, MouseButtonCallback);
    glfwSetScrollCallback(Window, ScrollCallback);
    glfwSetWindowSizeCallback(Window, WindowSizeCallback);
    glfwSetKeyCallback(Window, KeyCallback);

    App.Initialize(glfwGetWin32Window(Window));

    while (!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();

        App.Render();

        glfwSwapBuffers(Window);
    }

    App.Destroy();
}