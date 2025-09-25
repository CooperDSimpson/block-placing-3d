// main.cpp
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include "../engine-thingy/libs/libber.hpp"
// --------------------
// FPS Camera
// --------------------


// --------------------
// Shaders (basic color)
// --------------------
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.8,0.3,0.3,1.0);
})";

// --------------------
// Cube vertices
// --------------------
float cubeVertices[] = {
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,0.5f,-0.5f,
     0.5f,0.5f,-0.5f,  -0.5f,0.5f,-0.5f, -0.5f,-0.5f,-0.5f,
    -0.5f,-0.5f,0.5f,   0.5f,-0.5f,0.5f,   0.5f,0.5f,0.5f,
     0.5f,0.5f,0.5f,   -0.5f,0.5f,0.5f,  -0.5f,-0.5f,0.5f,
    -0.5f,0.5f,0.5f,   -0.5f,0.5f,-0.5f,  -0.5f,-0.5f,-0.5f,
    -0.5f,-0.5f,-0.5f, -0.5f,-0.5f,0.5f,  -0.5f,0.5f,0.5f,
     0.5f,0.5f,0.5f,    0.5f,0.5f,-0.5f,   0.5f,-0.5f,-0.5f,
     0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,    0.5f,0.5f,0.5f,
    -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f,0.5f,
     0.5f,-0.5f,0.5f,  -0.5f,-0.5f,0.5f,  -0.5f,-0.5f,-0.5f,
    -0.5f,0.5f,-0.5f,   0.5f,0.5f,-0.5f,   0.5f,0.5f,0.5f,
     0.5f,0.5f,0.5f,  -0.5f,0.5f,0.5f,  -0.5f,0.5f,-0.5f
};

// --------------------
// Globals for input
// --------------------
Camera camera;

Keyboard keyboard;

float deltaTime = 0.0f, lastFrame = 0.0f;


// --------------------
// Main
// --------------------
int main() {
    if(!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800,600,"FPS OpenGL",nullptr,nullptr);
    if(!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glewExperimental = true;
    if(glewInit()!=GLEW_OK) { std::cout<<"GLEW init failed\n"; return -1; }

    glfwSetCursorPosCallback(window, camera.mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader,1,&vertexShaderSource,nullptr);
    glCompileShader(vertexShader);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader,1,&fragmentShaderSource,nullptr);
    glCompileShader(fragmentShader);
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram,vertexShader);
    glAttachShader(shaderProgram,fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader); glDeleteShader(fragmentShader);

    // Cube VAO
    GLuint VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVertices),cubeVertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glEnable(GL_DEPTH_TEST);

    // Main loop
    while(!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        keyboard.Update(window);    

        float speed = 3.0f * deltaTime;
        Vec3 right = camera.front().cross(Vec3(0,1,0)).normalize();
        if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.pos = camera.pos + camera.front() * speed;
        if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.pos = camera.pos - camera.front() * speed;
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.pos = camera.pos - right * speed;
        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.pos = camera.pos + right * speed;
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) goto cleanup;

        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        GLuint loc = glGetUniformLocation(shaderProgram,"uMVP");
        glUseProgram(shaderProgram);
        Mat4 model = Mat4::identity();
        Mat4 view = camera.getViewMatrix();
        Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f, 800.0f/600.0f, 0.1f, 100.0f);

        Mat4 mvp = multiply(proj, multiply(view, model));
        glUniformMatrix4fv(loc, 1, GL_FALSE, mvp.m);
                // Simple MVP multiplication (column-major)
        Mat4 mvpFull = {};
        for(int i=0;i<4;i++)
            for(int j=0;j<4;j++)
                mvpFull.m[i + j*4] = proj.m[i]*view.m[j] + mvp.m[i]*1.0f; // simplified
        glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES,0,36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    cleanup:

    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}

