#include <stdlib.h>
#include <stdio.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

GLFWwindow* g_pWindow;
int g_width;
int g_height;

static void window_size_callback(GLFWwindow* window, int width, int height);
static void update();
static void draw();

int main(int argc, char *argv[])
{
	if (glfwInit() == GLFW_FALSE) return EXIT_FAILURE;

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	glfwWindowHint(GLFW_SAMPLES, 4);
	g_pWindow = glfwCreateWindow(800, 600, "Learn Chipmunk", NULL, NULL);
	if (g_pWindow == NULL) return EXIT_FAILURE;

	glfwMakeContextCurrent(g_pWindow);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return EXIT_FAILURE;

	glfwGetWindowSize(g_pWindow, &g_width, &g_height);

	GLFWwindowsizefun prevWindowSizeCallback = glfwSetWindowSizeCallback(g_pWindow, window_size_callback);

	glfwSwapInterval(1);
	while (glfwWindowShouldClose(g_pWindow) == GLFW_FALSE)
	{
		update();
		draw();

		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();
	}

	glfwSetWindowSizeCallback(g_pWindow, prevWindowSizeCallback);

	glfwDestroyWindow(g_pWindow);
	glfwTerminate();
	return EXIT_SUCCESS;
}

static void update()
{

}

static void draw()
{
	glViewport(0, 0, g_width, g_height);
	glClearColor(0.0f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void window_size_callback(GLFWwindow* window, int width, int height)
{
	g_width = width;
	g_height = height;
	printf("window_size_callback %d %d\n", width, height);
}
