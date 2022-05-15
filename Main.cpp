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

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

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

    App.Initialize(glfwGetWin32Window(Window));

    while (!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();

        App.Render();

        glfwSwapBuffers(Window);
    }

    App.Destroy();
}