#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <chipmunk/chipmunk.h>
#include <chipmunk/chipmunk_private.h>

#define GL_CHECK(_call) \
	_call; \
	{ GLenum err; \
	while ((err = glGetError()) != GL_NO_ERROR) { \
		printf("GL error: %d in %s %s %d - for %s\n", err, __FILE__, __FUNCTION__, __LINE__, #_call); \
	}}

#define GL_CHECK_RETURN(_call) \
	_call; \
	{ bool iserr = false; { GLenum err; \
	while ((err = glGetError()) != GL_NO_ERROR) { \
		iserr = true; printf("GL error: %d in %s %s %d - for %s\n", err, __FILE__, __FUNCTION__, __LINE__, #_call); \
	}} if (iserr) return false; }

#define VBO_MAX_SIZE (13*4096)
#define IBO_MAX_SIZE (6*4096/4)

const char* vertexShaderSource =
"#version 460 core\n\
\n\
layout (location = 0) in vec2 aPos;\n\
layout (location = 1) in vec2 aUV;\n\
layout (location = 2) in float aRadius;\n\
layout (location = 3) in vec4 aFill;\n\
layout (location = 4) in vec4 aOutline;\n\
\n\
uniform vec2 scale;\n\
\n\
out vec2 vUv;\n\
out vec4 vFill;\n\
out vec4 vOutline;\n\
\n\
void main()\n\
{\n\
	vUv = aUV;\n\
	vFill = aFill;\n\
	vFill.rgb *= aFill.a;\n\
	vOutline = aOutline;\n\
	vOutline.rgb *= aOutline.a;\n\
	vec2 pos = aPos + aUV*aRadius;\n\
	gl_Position = vec4(pos*scale - vec2(1.0, 1.0), 0.0, 1.0);\n\
}\n\
";

const char* fragmentShaderSource =
"#version 460 core\n\
\n\
in vec2 vUv;\n\
in vec4 vFill;\n\
in vec4 vOutline;\n\
\n\
out vec4 FragColor;\n\
\n\
void main()\n\
{\n\
	float len = length(vUv);\n\
	float fw = length(fwidth(vUv));\n\
	float mask = smoothstep(-1, fw - 1, -len);\n\
	float outline = 1 - fw;\n\
	float outline_mask = smoothstep(outline - fw, outline, len);\n\
	vec4 color = vFill + (vOutline - vFill * vOutline.a) * outline_mask;\n\
	FragColor = color*mask;\n\
}\n\
";

GLFWwindow* g_pWindow;
int g_width;
int g_height;

cpSpace* g_pSpace;
cpShape* g_pGroundShape;
cpBody* g_pBallBody;
cpShape* g_pBallShape;
cpBody* g_pBoxBody;
cpShape* g_pBoxShape;
cpSpaceDebugDrawOptions g_spaceDebugDrawOptions;

GLuint g_program;
GLuint g_vao;
GLuint g_vbo;
GLuint g_ebo;
GLint g_scaleLoc;

float          g_vertices[VBO_MAX_SIZE];
unsigned short g_indices[IBO_MAX_SIZE];
size_t g_verticesCount;
size_t g_indicesCount;

static void window_size_callback(GLFWwindow* window, int width, int height);
static void update(float deltaTime);
static void draw();

static void drawCircle(cpVect p, cpFloat a, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data);
static void drawSegment(cpVect a, cpVect b, cpSpaceDebugColor color, cpDataPointer data);
static void drawFatSegment(cpVect a, cpVect b, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data);
static void drawPolygon(int count, const cpVect* verts, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data);
static void drawDot(cpFloat size, cpVect pos, cpSpaceDebugColor color, cpDataPointer data);
static cpSpaceDebugColor colorForShape(cpShape* pShape, cpDataPointer data);

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

	GLuint vertexShader = GL_CHECK_RETURN(glCreateShader(GL_VERTEX_SHADER));
	GL_CHECK(glShaderSource(vertexShader, 1, &vertexShaderSource, NULL));
	GL_CHECK(glCompileShader(vertexShader));
	{
		GLint success;
		GL_CHECK(glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success));
		if (!success)
		{
			GLint infoLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &infoLength);
			char* infoLog = (char*)malloc(infoLength*sizeof(char*));
			GL_CHECK(glGetShaderInfoLog(vertexShader, infoLength, &infoLength, infoLog));
			printf("ERROR::SHADER_COMPILATION_ERROR of type : VERTEX_SHADER\n%s\n -- --------------------------------------------------- -- \n", infoLog);
			free(infoLog);
			GL_CHECK(glDeleteShader(vertexShader));
		}
	}

	GLuint fragmentShader = GL_CHECK_RETURN(glCreateShader(GL_FRAGMENT_SHADER));
	GL_CHECK(glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL));
	GL_CHECK(glCompileShader(fragmentShader));
	{
		GLint success;
		GL_CHECK(glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success));
		if (!success)
		{
			GLint infoLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &infoLength);
			char* infoLog = (char*)malloc(infoLength * sizeof(char*));
			GL_CHECK(glGetShaderInfoLog(fragmentShader, infoLength, &infoLength, infoLog));
			printf("ERROR::SHADER_COMPILATION_ERROR of type : FRAGMENT_SHADER\n%s\n -- --------------------------------------------------- -- \n", infoLog);
			free(infoLog);
			GL_CHECK(glDeleteShader(fragmentShader));
		}
	}

	g_program = GL_CHECK_RETURN(glCreateProgram());
	GL_CHECK(glAttachShader(g_program, vertexShader));
	GL_CHECK(glAttachShader(g_program, fragmentShader));
	GL_CHECK(glLinkProgram(g_program));

	GL_CHECK(glDeleteShader(vertexShader));
	GL_CHECK(glDeleteShader(fragmentShader));

	glGenVertexArrays(1, &g_vao);
	glBindVertexArray(g_vao);
		glGenBuffers(1, &g_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
			glBufferData(GL_ARRAY_BUFFER, VBO_MAX_SIZE*sizeof(float), NULL, GL_DYNAMIC_DRAW);
			glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(GLfloat), (GLvoid*)0);

			glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 13 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));

			glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 13 * sizeof(GLfloat), (GLvoid*)(4 * sizeof(GLfloat)));

			glEnableVertexAttribArray(3);
				glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 13 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));

			glEnableVertexAttribArray(4);
				glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 13 * sizeof(GLfloat), (GLvoid*)(9 * sizeof(GLfloat)));

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glGenBuffers(1, &g_ebo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, IBO_MAX_SIZE*sizeof(unsigned short), NULL, GL_DYNAMIC_DRAW);
	glBindVertexArray(0);

	glUseProgram(g_program);
	g_scaleLoc = glGetUniformLocation(g_program, "scale");

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	g_pSpace = cpSpaceNew();
	cpSpaceSetGravity(g_pSpace, cpv(0.0, -100.0));

	//g_pGroundShape = cpSegmentShapeNew(cpSpaceGetStaticBody(g_pSpace), cpv(20, 100), cpv(600, 50), 0);
	g_pGroundShape = cpSegmentShapeNew(cpSpaceGetStaticBody(g_pSpace), cpv(20, 100), cpv(600, 100), 0);
	cpShapeSetFriction(g_pGroundShape, 0.6);
	//cpShapeSetElasticity(g_pGroundShape, 0.6);
	cpShapeSetElasticity(g_pGroundShape, 1.0);
	cpSpaceAddShape(g_pSpace, g_pGroundShape);

	g_pBallBody = cpSpaceAddBody(g_pSpace, cpBodyNew(1.0, cpMomentForCircle(10, 0, 50, cpvzero)));
	cpBodySetPosition(g_pBallBody, cpv(160.0, 300.0));

	g_pBallShape = cpSpaceAddShape(g_pSpace, cpCircleShapeNew(g_pBallBody, 50, cpvzero));
	cpShapeSetFriction(g_pBallShape, 0.6);
	cpShapeSetElasticity(g_pBallShape, 1.0);

	g_pBoxBody = cpSpaceAddBody(g_pSpace, cpBodyNew(1.0, cpMomentForBox(10, 50, 100)));
	cpBodySetPosition(g_pBoxBody, cpv(300.0, 300.0));
	cpBodySetAngle(g_pBoxBody, -45.0);

	g_pBoxShape = cpSpaceAddShape(g_pSpace, cpBoxShapeNew(g_pBoxBody, 50, 100, 0.0));
	cpShapeSetFriction(g_pBoxShape, 0.6);
	cpShapeSetElasticity(g_pBoxShape, 0.8);

	g_spaceDebugDrawOptions.drawCircle          = drawCircle;
	g_spaceDebugDrawOptions.drawSegment         = drawSegment;
	g_spaceDebugDrawOptions.drawFatSegment      = drawFatSegment;
	g_spaceDebugDrawOptions.drawPolygon         = drawPolygon;
	g_spaceDebugDrawOptions.drawDot             = drawDot;
	g_spaceDebugDrawOptions.flags               = (cpSpaceDebugDrawFlags)(CP_SPACE_DEBUG_DRAW_SHAPES | CP_SPACE_DEBUG_DRAW_CONSTRAINTS | CP_SPACE_DEBUG_DRAW_COLLISION_POINTS);
	g_spaceDebugDrawOptions.shapeOutlineColor   = (cpSpaceDebugColor){ 0xEE / 255.0f, 0xE8 / 255.0f, 0xD5 / 255.0f, 1.0f };
	g_spaceDebugDrawOptions.colorForShape       = colorForShape;
	g_spaceDebugDrawOptions.constraintColor     = (cpSpaceDebugColor){ 0.0f, 0.75f, 0.0f, 1.0f };
	g_spaceDebugDrawOptions.collisionPointColor = (cpSpaceDebugColor){ 1.0f, 0.0f, 0.0f, 1.0f };
	g_spaceDebugDrawOptions.data                = NULL;

	GLfloat deltaTime = 0.0f;
	GLfloat lastFrame = 0.0f;
	glfwSwapInterval(1);
	while (glfwWindowShouldClose(g_pWindow) == GLFW_FALSE)
	{
		GLfloat currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		update(deltaTime);
		draw();

		glfwSwapBuffers(g_pWindow);
		glfwPollEvents();
	}

	//cpShapeFree(g_pBoxShape);
	//cpBodyFree(g_pBoxBody);
	//cpShapeFree(g_pBallShape);
	//cpBodyFree(g_pBallBody);
	//cpShapeFree(g_pGroundShape);
	cpSpaceFree(g_pSpace);

	glDeleteBuffers(1, &g_vbo);
	glDeleteBuffers(1, &g_ebo);
	glDeleteVertexArrays(1, &g_vao);
	glDeleteProgram(g_program);

	glfwSetWindowSizeCallback(g_pWindow, prevWindowSizeCallback);

	glfwDestroyWindow(g_pWindow);
	glfwTerminate();
	return EXIT_SUCCESS;
}

static void update(float deltaTime)
{
	cpSpaceStep(g_pSpace, deltaTime);

	g_verticesCount = 0;
	g_indicesCount = 0;

	cpSpaceDebugDraw(g_pSpace, &g_spaceDebugDrawOptions);

	{
		glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
		void* pBuffer = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
		memcpy(pBuffer, g_vertices, g_verticesCount*13*sizeof(float));
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
		void* pBuffer = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
		memcpy(pBuffer, g_indices, g_indicesCount *sizeof(unsigned short));
		glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	}
}

static void draw()
{
	glViewport(0, 0, g_width, g_height);
	glClearColor(0.0f, 0.2f, 0.2f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(g_program);
	glBindVertexArray(g_vao);
	glUniform2f(g_scaleLoc, 2.0f / g_width, 2.0f / g_height);
	glDrawElements(GL_TRIANGLES, g_indicesCount, GL_UNSIGNED_SHORT, 0);
}

static void window_size_callback(GLFWwindow* window, int width, int height)
{
	g_width = width;
	g_height = height;
	printf("window_size_callback %d %d\n", width, height);
}

static void setVertex(size_t index, cpVect pos, cpVect uv, float r, cpSpaceDebugColor fill, cpSpaceDebugColor outline)
{
	g_vertices[index * 13 + 0]  = pos.x;
	g_vertices[index * 13 + 1]  = pos.y;
	g_vertices[index * 13 + 2]  = uv.x;
	g_vertices[index * 13 + 3]  = uv.y;
	g_vertices[index * 13 + 4]  = r;
	g_vertices[index * 13 + 5]  = fill.r;
	g_vertices[index * 13 + 6]  = fill.g;
	g_vertices[index * 13 + 7]  = fill.b;
	g_vertices[index * 13 + 8]  = fill.a;
	g_vertices[index * 13 + 9]  = outline.r;
	g_vertices[index * 13 + 10] = outline.g;
	g_vertices[index * 13 + 11] = outline.b;
	g_vertices[index * 13 + 12] = outline.a;
}

static void drawCircle(cpVect p, cpFloat a, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	float r = radius + 1.0f;

	size_t startVertexIndex = g_verticesCount;
	setVertex(g_verticesCount++, p, cpv(-1.0, -1.0), r, fill, outline);
	setVertex(g_verticesCount++, p, cpv(-1.0, +1.0), r, fill, outline);
	setVertex(g_verticesCount++, p, cpv(+1.0, +1.0), r, fill, outline);
	setVertex(g_verticesCount++, p, cpv(+1.0, -1.0), r, fill, outline);

	size_t indexIndex = g_indicesCount;
	g_indicesCount += 6;
	g_indices[indexIndex + 0] = startVertexIndex + 0;
	g_indices[indexIndex + 1] = startVertexIndex + 1;
	g_indices[indexIndex + 2] = startVertexIndex + 2;
	g_indices[indexIndex + 3] = startVertexIndex + 0;
	g_indices[indexIndex + 4] = startVertexIndex + 2;
	g_indices[indexIndex + 5] = startVertexIndex + 3;
}

static void drawSegment(cpVect a, cpVect b, cpSpaceDebugColor color, cpDataPointer data)
{
	drawFatSegment(a, b, 0.0f, color, color, data);
}

static void drawFatSegment(cpVect a, cpVect b, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	float r = radius + 1.0f;
	cpVect t = cpvnormalize(cpvsub(b, a));

	size_t startVertexIndex = g_verticesCount;
	setVertex(g_verticesCount++, a, cpv(-t.x + t.y, -t.x - t.y), r, fill, outline);
	setVertex(g_verticesCount++, a, cpv(-t.x - t.y, +t.x - t.y), r, fill, outline);
	setVertex(g_verticesCount++, a, cpv(-0.0 + t.y, -t.x + 0.0), r, fill, outline);
	setVertex(g_verticesCount++, a, cpv(-0.0 - t.y, +t.x + 0.0), r, fill, outline);

	setVertex(g_verticesCount++, b, cpv(+0.0 + t.y, -t.x - 0.0), r, fill, outline);
	setVertex(g_verticesCount++, b, cpv(+0.0 - t.y, +t.x - 0.0), r, fill, outline);
	setVertex(g_verticesCount++, b, cpv(+t.x + t.y, -t.x + t.y), r, fill, outline);
	setVertex(g_verticesCount++, b, cpv(+t.x - t.y, +t.x + t.y), r, fill, outline);

	size_t indexIndex = g_indicesCount;
	g_indicesCount += 18;

	g_indices[indexIndex + 0]  = startVertexIndex + 0;
	g_indices[indexIndex + 1]  = startVertexIndex + 1;
	g_indices[indexIndex + 2]  = startVertexIndex + 2;
	g_indices[indexIndex + 3]  = startVertexIndex + 1;
	g_indices[indexIndex + 4]  = startVertexIndex + 2;
	g_indices[indexIndex + 5]  = startVertexIndex + 3;
	g_indices[indexIndex + 6]  = startVertexIndex + 2;
	g_indices[indexIndex + 7]  = startVertexIndex + 3;
	g_indices[indexIndex + 8]  = startVertexIndex + 4;
	g_indices[indexIndex + 9]  = startVertexIndex + 3;
	g_indices[indexIndex + 10] = startVertexIndex + 4;
	g_indices[indexIndex + 11] = startVertexIndex + 5;
	g_indices[indexIndex + 12] = startVertexIndex + 4;
	g_indices[indexIndex + 13] = startVertexIndex + 5;
	g_indices[indexIndex + 14] = startVertexIndex + 6;
	g_indices[indexIndex + 15] = startVertexIndex + 5;
	g_indices[indexIndex + 16] = startVertexIndex + 6;
	g_indices[indexIndex + 17] = startVertexIndex + 7;
}

static void drawPolygon(int count, const cpVect* verts, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	float inset = -cpfmax(0.0, 2.0 - radius);
	float outset = radius + 1.0f;
	float r = outset - inset;

	size_t startVertexIndex = g_verticesCount;
	for (int i = 0; i < count; i++)
	{
		cpVect v0 = verts[i];
		cpVect v_prev = verts[(i + (count - 1)) % count];
		cpVect v_next = verts[(i + (count + 1)) % count];

		cpVect n1 = cpvnormalize(cpvrperp(cpvsub(v0, v_prev)));
		cpVect n2 = cpvnormalize(cpvrperp(cpvsub(v_next, v0)));
		cpVect of = cpvmult(cpvadd(n1, n2), 1.0 / (cpvdot(n1, n2) + 1.0f));
		cpVect v = cpvadd(v0, cpvmult(of, inset));

		setVertex(g_verticesCount++, v, cpv(0.0f, 0.0f), 0.0f, fill, outline);
		setVertex(g_verticesCount++, v, cpv(n1.x, n1.y), r, fill, outline);
		setVertex(g_verticesCount++, v, cpv(of.x, of.y), r, fill, outline);
		setVertex(g_verticesCount++, v, cpv(n2.x, n2.y), r, fill, outline);
	}

	size_t indexIndex = g_indicesCount;
	g_indicesCount += 3 * (5 * count - 2);

	// Polygon fill triangles.
	for (int i = 0; i < count - 2; i++)
	{
		g_indices[indexIndex + 3 * i + 0] = startVertexIndex + 0;
		g_indices[indexIndex + 3 * i + 1] = startVertexIndex + 4 * (i + 1);
		g_indices[indexIndex + 3 * i + 2] = startVertexIndex + 4 * (i + 2);
	}

	indexIndex += 3 * (count - 2);
	for (int i0 = 0; i0 < count; i0++)
	{
		int i1 = (i0 + 1) % count;
		g_indices[indexIndex + 12 * i0 + 0]  = startVertexIndex + 4 * i0 + 0;
		g_indices[indexIndex + 12 * i0 + 1]  = startVertexIndex + 4 * i0 + 1;
		g_indices[indexIndex + 12 * i0 + 2]  = startVertexIndex + 4 * i0 + 2;
		g_indices[indexIndex + 12 * i0 + 3]  = startVertexIndex + 4 * i0 + 0;
		g_indices[indexIndex + 12 * i0 + 4]  = startVertexIndex + 4 * i0 + 2;
		g_indices[indexIndex + 12 * i0 + 5]  = startVertexIndex + 4 * i0 + 3;
		g_indices[indexIndex + 12 * i0 + 6]  = startVertexIndex + 4 * i0 + 0;
		g_indices[indexIndex + 12 * i0 + 7]  = startVertexIndex + 4 * i0 + 3;
		g_indices[indexIndex + 12 * i0 + 8]  = startVertexIndex + 4 * i1 + 0;
		g_indices[indexIndex + 12 * i0 + 9]  = startVertexIndex + 4 * i0 + 3;
		g_indices[indexIndex + 12 * i0 + 10] = startVertexIndex + 4 * i1 + 0;
		g_indices[indexIndex + 12 * i0 + 11] = startVertexIndex + 4 * i1 + 1;
	}
}

static void drawDot(cpFloat size, cpVect pos, cpSpaceDebugColor color, cpDataPointer data)
{
	float r = size * 0.5f;

	size_t startVertexIndex = g_verticesCount;
	setVertex(g_verticesCount++, pos, cpv(-1.0f, -1.0f), r, color, color);
	setVertex(g_verticesCount++, pos, cpv(-1.0f, +1.0f), r, color, color);
	setVertex(g_verticesCount++, pos, cpv(+1.0f, +1.0f), r, color, color);
	setVertex(g_verticesCount++, pos, cpv(+1.0f, -1.0f), r, color, color);

	size_t indexIndex = g_indicesCount;
	g_indicesCount += 6;
	g_indices[indexIndex + 0] = startVertexIndex + 0;
	g_indices[indexIndex + 1] = startVertexIndex + 1;
	g_indices[indexIndex + 2] = startVertexIndex + 2;
	g_indices[indexIndex + 3] = startVertexIndex + 0;
	g_indices[indexIndex + 4] = startVertexIndex + 2;
	g_indices[indexIndex + 5] = startVertexIndex + 3;
}

static cpSpaceDebugColor Colors[] =
{
	{0xb5 / 255.0f, 0x89 / 255.0f, 0x00 / 255.0f, 1.0f},
	{0xcb / 255.0f, 0x4b / 255.0f, 0x16 / 255.0f, 1.0f},
	{0xdc / 255.0f, 0x32 / 255.0f, 0x2f / 255.0f, 1.0f},
	{0xd3 / 255.0f, 0x36 / 255.0f, 0x82 / 255.0f, 1.0f},
	{0x6c / 255.0f, 0x71 / 255.0f, 0xc4 / 255.0f, 1.0f},
	{0x26 / 255.0f, 0x8b / 255.0f, 0xd2 / 255.0f, 1.0f},
	{0x2a / 255.0f, 0xa1 / 255.0f, 0x98 / 255.0f, 1.0f},
	{0x85 / 255.0f, 0x99 / 255.0f, 0x00 / 255.0f, 1.0f},
};

static cpSpaceDebugColor colorForShape(cpShape* pShape, cpDataPointer data)
{
	if (cpShapeGetSensor(pShape))
	{
		return (cpSpaceDebugColor){ 1.0f, 1.0f, 1.0f, 0.1f };
	}
	else
	{
		cpBody* pBody = cpShapeGetBody(pShape);

		if (cpBodyIsSleeping(pBody))
		{
			return (cpSpaceDebugColor) { 0x58 / 255.0f, 0x6e / 255.0f, 0x75 / 255.0f, 1.0f };
		}
		else if (pBody->sleeping.idleTime > pShape->space->sleepTimeThreshold)
		{
			return (cpSpaceDebugColor) { 0x93 / 255.0f, 0xa1 / 255.0f, 0xa1 / 255.0f, 1.0f };
		}
		else
		{
			uint32_t val = (uint32_t)pShape->hashid;

			// scramble the bits up using Robert Jenkins' 32 bit integer hash function
			val = (val + 0x7ed55d16) + (val << 12);
			val = (val ^ 0xc761c23c) ^ (val >> 19);
			val = (val + 0x165667b1) + (val << 5);
			val = (val + 0xd3a2646c) ^ (val << 9);
			val = (val + 0xfd7046c5) + (val << 3);
			val = (val ^ 0xb55a4f09) ^ (val >> 16);
			return Colors[val & 0x7];
		}
	}
}
