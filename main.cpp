#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <cstdlib>
#include "../engine-thingy/libs/libber.hpp"

// --------------------
// Shaders
// --------------------
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aBary;

out vec3 vBary;

uniform mat4 uMVP;

void main() {
    vBary = aBary;
    gl_Position = uMVP * vec4(aPos, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 330 core
in vec3 vBary;
out vec4 FragColor;

uniform vec3 uColor;

float edgeFactor() {
    vec3 d = fwidth(vBary);
    vec3 a3 = smoothstep(vec3(0.0), d*1.5, vBary);
    return min(min(a3.x,a3.y),a3.z);
}

void main() {
    float factor = edgeFactor();
    FragColor = mix(vec4(uColor,1.0), vec4(0,0,0,1.0), 1.0 - factor);
}
)";

// --------------------
// Cube data (same as before)
// --------------------
float cubeVertices[] = {
    // positions          // barycentric
    // Front
    -0.5f,-0.5f,0.5f, 1,0,0,  0.5f,-0.5f,0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
     0.5f,0.5f,0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0, -0.5f,-0.5f,0.5f, 0,0,1,
    // Back
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,-0.5f, 0,1,0,  0.5f,-0.5f,-0.5f, 0,0,1,
     0.5f,0.5f,-0.5f, 1,0,0, -0.5f,-0.5f,-0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,0,1,
    // Left
    -0.5f,-0.5f,-0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0, -0.5f,-0.5f,0.5f, 0,1,0, -0.5f,0.5f,0.5f, 0,0,1,
    // Right
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,-0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,0.5f, 0,1,0,  0.5f,-0.5f,0.5f, 0,0,1,
    // Top
    -0.5f,0.5f,-0.5f, 1,0,0,  0.5f,0.5f,0.5f, 0,1,0,  0.5f,0.5f,-0.5f, 0,0,1,
    -0.5f,0.5f,-0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
    // Bottom
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,-0.5f, 0,1,0,  0.5f,-0.5f,0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,0.5f, 0,1,0, -0.5f,-0.5f,0.5f, 0,0,1
};

// --------------------
// Globals
// --------------------
Camera camera;
Keyboard keyboard;
Mouse mouse;
float deltaTime=0,lastFrame=0;
bool wireframeMode = false;

// --------------------
// Cube class
// --------------------
struct Cube {
    Vec3 pos;
    Vec3 rot;   // rotation angles
    Vec3 color;
};

// Generate random cubes
std::vector<Cube> cubes;
void generateCubes(int n){
    for(int i=0;i<n;i++){
        Cube c;
        c.pos = Vec3((rand()%100-50),(rand()%100-50),(rand()%100-50));
        c.rot = Vec3(0,0,0);
        c.color = Vec3((rand()%100)/100.0f,(rand()%100)/100.0f,(rand()%100)/100.0f);
        cubes.push_back(c);
    }
}



// --------------------
// Main
// --------------------
int main(){
    Cube placementGuide;
    placementGuide.color = Vec3(1,1,1); // White outline
    
    if(!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800,600,"Mini FPS Game",nullptr,nullptr);
    if(!window){ glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glewExperimental=true;
    if(glewInit()!=GLEW_OK){ std::cout<<"GLEW init failed\n"; return -1; }

    glfwSetCursorPosCallback(window,camera.mouse_callback);
    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);

    // Shader compile
    GLuint vs=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs,1,&vertexShaderSource,nullptr); glCompileShader(vs);
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&fragmentShaderSource,nullptr); glCompileShader(fs);
    GLuint shaderProgram=glCreateProgram();
    glAttachShader(shaderProgram,vs); glAttachShader(shaderProgram,fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    // VAO/VBO
    GLuint VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVertices),cubeVertices,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);

    glEnable(GL_DEPTH_TEST);

    while(!glfwWindowShouldClose(window)){
        Vec3 forward = camera.front();
        placementGuide.pos = camera.pos + forward * 7.5f; // 3 units ahead
        placementGuide.pos.x = std::round(placementGuide.pos.x);
        placementGuide.pos.y = std::round(placementGuide.pos.y);
        placementGuide.pos.z = std::round(placementGuide.pos.z);

        float currentFrame=glfwGetTime();
        deltaTime=currentFrame-lastFrame;
        lastFrame=currentFrame;
        keyboard.Update(window);
        mouse.Update(window);

        // Movement
        float speed=5.0f*deltaTime;

        if(keyboard.curr_keys[GLFW_KEY_LEFT_SHIFT]){speed *= 2;}


        Vec3 right = camera.front().cross(Vec3(0,1,0)).normalize();
        if(glfwGetKey(window,GLFW_KEY_W)==GLFW_PRESS) camera.pos=camera.pos+camera.front()*speed;
        if(glfwGetKey(window,GLFW_KEY_S)==GLFW_PRESS) camera.pos=camera.pos-camera.front()*speed;
        if(glfwGetKey(window,GLFW_KEY_A)==GLFW_PRESS) camera.pos=camera.pos-right*speed;
        if(glfwGetKey(window,GLFW_KEY_D)==GLFW_PRESS) camera.pos=camera.pos+right*speed;
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) break;
        if(!keyboard.prev_keys['F'] && keyboard.curr_keys['F']) wireframeMode = !wireframeMode;
        
        

        if(false){
            for(Cube &billy: cubes){
                billy.pos.x += 1 * deltaTime;
                billy.pos.z = sin(billy.pos.x);
                billy.pos.y = cos(billy.pos.x);
            }
        }
        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_RIGHT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_RIGHT]){
             for(auto it = cubes.begin(); it != cubes.end(); ++it){
                if(it->pos.x == placementGuide.pos.x &&
                   it->pos.y == placementGuide.pos.y &&
                   it->pos.z == placementGuide.pos.z){
                    cubes.erase(it); // Remove it 
                    break;
                }
            }           
        }

        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_LEFT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_LEFT]){
            Cube c;
            c.pos = placementGuide.pos;
            c.rot = Vec3(0,0,0);
            c.color = Vec3((rand()%100)/100.0f,(rand()%100)/100.0f,(rand()%100)/100.0f);
            cubes.push_back(c);
        }



        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        GLuint loc = glGetUniformLocation(shaderProgram,"uMVP");
        GLuint colorLoc = glGetUniformLocation(shaderProgram,"uColor");

        for(auto& c: cubes){
            // Simple rotation
            
            Mat4 model = Mat4::identity();
            model = multiply(model, model.translate(c.pos));
            model = multiply(model, model.rotateY(c.rot.y));
            Mat4 view = camera.getViewMatrix();
            Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,800.0f/600.0f,0.1f,100.0f);
            Mat4 mvp = multiply(proj, multiply(view, model));
            glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
            glUniform3f(colorLoc,c.color.x,c.color.y,c.color.z);

            glBindVertexArray(VAO);
            if(wireframeMode) glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            else glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glDrawArrays(GL_TRIANGLES,0,36*6);
        }

        glUniform3f(colorLoc, placementGuide.color.x, placementGuide.color.y, placementGuide.color.z);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // Always wireframe
        Mat4 model = Mat4::identity();
        model = multiply(model, model.translate(placementGuide.pos));
        Mat4 view = camera.getViewMatrix();
        Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,800.0f/600.0f,0.1f,100.0f);
        Mat4 mvp = multiply(proj, multiply(view, model));
        glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES,0,36*6);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
