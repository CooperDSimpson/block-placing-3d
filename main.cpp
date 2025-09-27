#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm> // for std::find_if
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

uniform vec3 uColor;       // fill color for normal cubes
uniform bool uPlacement;   // true = placement guide, false = normal cubes
uniform float uTime;       // time for rainbow effect

float edgeFactor() {
    vec3 d = fwidth(vBary);
    vec3 a3 = smoothstep(vec3(0.0), d*1.5, vBary);
    return min(min(a3.x,a3.y),a3.z);
}

vec3 rainbow(float t) {
    return vec3(
        0.5 + 0.5 * sin(t * 2.0),
        0.5 + 0.5 * sin(t * 2.0 + 2.0),
        0.5 + 0.5 * sin(t * 2.0 + 4.0)
    );
}

void main() {
    float factor = edgeFactor();

    if(uPlacement) {
        // Rainbow outline only, transparent inside
        vec3 outlineColor = rainbow(uTime);
        FragColor = vec4(outlineColor, 1.0 - factor); 
    } else {
        // Regular cubes: black outline + fill
        vec3 outlineColor = vec3(0.0,0.0,0.0);
        vec3 color = mix(outlineColor, uColor, factor);
        FragColor = vec4(color, 1.0);
    }
}


)";

const char* vertexOutlineShader = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char* fragmentOutlineShader = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;  // color to invert

void main(){
    FragColor = vec4(vec3(1.0) - uColor, 1.0); // inverted color
}
)";

// Crosshair shaders
const char* crosshairVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

const char* crosshairFragmentShader = R"(
#version 330 core
out vec4 FragColor;
uniform sampler2D uFramebuffer;
uniform vec2 uScreenSize;

void main() {
    // Get screen coordinates
    vec2 screenCoord = gl_FragCoord.xy / uScreenSize;
    
    // Sample the color behind the crosshair
    vec3 backgroundColor = texture(uFramebuffer, screenCoord).rgb;
    
    // Calculate inverted color
    vec3 invertedColor = vec3(1.0) - backgroundColor;
    
    // Ensure good contrast - if too close to gray, make it white or black
    float brightness = dot(backgroundColor, vec3(0.299, 0.587, 0.114));
    if (brightness > 0.4 && brightness < 0.6) {
        invertedColor = brightness > 0.5 ? vec3(0.0) : vec3(1.0);
    }
    
    FragColor = vec4(invertedColor, 1.0);
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

// Crosshair vertices (2D screen space)
float crosshairVertices[] = {
    // Horizontal line
    -0.02f,  0.0f,
     0.02f,  0.0f,
    // Vertical line  
     0.0f, -0.02f,
     0.0f,  0.02f
};

// --------------------
// Globals
// --------------------
Camera3d camera;
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

// Ray = origin + dir * t
bool rayIntersectsCube(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& cubePos, float& tNear) {
    Vec3 min = cubePos - Vec3(0.5f,0.5f,0.5f);
    Vec3 max = cubePos + Vec3(0.5f,0.5f,0.5f);

    float t1 = (min.x - rayOrigin.x) / rayDir.x;
    float t2 = (max.x - rayOrigin.x) / rayDir.x;
    float t3 = (min.y - rayOrigin.y) / rayDir.y;
    float t4 = (max.y - rayOrigin.y) / rayDir.y;
    float t5 = (min.z - rayOrigin.z) / rayDir.z;
    float t6 = (max.z - rayOrigin.z) / rayDir.z;

    float tmin = std::fmax(std::fmax(std::fmin(t1,t2), std::fmin(t3,t4)), std::fmin(t5,t6));
    float tmax = std::fmin(std::fmin(std::fmax(t1,t2), std::fmax(t3,t4)), std::fmax(t5,t6));

    if(tmax < 0 || tmin > tmax) return false;

    tNear = tmin;
    return true;
}

Cube* getCubeUnderCursor(Vec3 rayOrigin, Vec3 rayDir, float maxDistance, Vec3& hitPos, Vec3& hitNormal) {
    Cube* closest = nullptr;
    float closestT = maxDistance;

    for(auto& c : cubes){
        float t;
        if(rayIntersectsCube(rayOrigin, rayDir, c.pos, t)){
            if(t < closestT){
                closestT = t;
                closest = &c;
            }
        }
    }

    if(closest){
        hitPos = rayOrigin + rayDir * closestT;

        // Determine which face was hit
        Vec3 local = hitPos - closest->pos; // Cube-centered coordinates (-0.5..0.5)
        float absX = std::abs(local.x), absY = std::abs(local.y), absZ = std::abs(local.z);
        if(absX > absY && absX > absZ) hitNormal = Vec3(local.x>0?1:-1,0,0);
        else if(absY > absX && absY > absZ) hitNormal = Vec3(0,local.y>0?1:-1,0);
        else hitNormal = Vec3(0,0,local.z>0?1:-1);

        return closest;
    }

    return nullptr;
}


// --------------------
// Main
// --------------------
int main(){

    Cube placementGuide;
    
    if(!glfwInit()) return -1;
    
    // Get the primary monitor and its video mode for fullscreen
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    // Create fullscreen window
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Mini FPS Game", monitor, nullptr);
    if(!window){ glfwTerminate(); return -1; }
    
    // Store screen dimensions for later use
    int screenWidth = mode->width;
    int screenHeight = mode->height;
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
      
    // --- Compile outline shader ---
    GLuint vsOutline = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsOutline, 1, &vertexOutlineShader, nullptr);
    glCompileShader(vsOutline);

    GLuint fsOutline = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsOutline, 1, &fragmentOutlineShader, nullptr);
    glCompileShader(fsOutline);

    GLuint outlineShaderProgram = glCreateProgram();
    glAttachShader(outlineShaderProgram, vsOutline);
    glAttachShader(outlineShaderProgram, fsOutline);
    glLinkProgram(outlineShaderProgram);

    glDeleteShader(vsOutline);
    glDeleteShader(fsOutline);

    // --- Compile crosshair shader ---
    GLuint vsCrosshair = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsCrosshair, 1, &crosshairVertexShader, nullptr);
    glCompileShader(vsCrosshair);

    GLuint fsCrosshair = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsCrosshair, 1, &crosshairFragmentShader, nullptr);
    glCompileShader(fsCrosshair);

    GLuint crosshairShaderProgram = glCreateProgram();
    glAttachShader(crosshairShaderProgram, vsCrosshair);
    glAttachShader(crosshairShaderProgram, fsCrosshair);
    glLinkProgram(crosshairShaderProgram);

    glDeleteShader(vsCrosshair);
    glDeleteShader(fsCrosshair);

    // Setup crosshair VAO
    GLuint crosshairVAO, crosshairVBO;
    glGenVertexArrays(1, &crosshairVAO);
    glGenBuffers(1, &crosshairVBO);
    glBindVertexArray(crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshairVertices), crosshairVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Setup framebuffer for crosshair color sampling
    GLuint framebuffer, colorTexture;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colorTexture);
    
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    
    GLuint depthBuffer;
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "Framebuffer not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    while(!glfwWindowShouldClose(window)){
        float t = glfwGetTime()*10; // seconds since program started
        placementGuide.color.x = 0.5f + 0.5f * sin(t * 2.0f); // red
        placementGuide.color.y = 0.5f + 0.5f * sin(t * 2.0f + 2.0f); // green, phase shifted
        placementGuide.color.z = 0.5f + 0.5f * sin(t * 2.0f + 4.0f); // blue, phase shifted

        Vec3 forward = camera.front();
        Vec3 hitPos, hitNormal;
        Cube* target = getCubeUnderCursor(camera.pos, camera.front(), 7.5f, hitPos, hitNormal);
        Vec3 placementPos;
        if(target){
            placementPos = target->pos; // place on the face
        } else {
            placementPos = camera.pos + camera.front() * 7.5f;
        }
        placementGuide.pos = Vec3(std::round(placementPos.x),
                                  std::round(placementPos.y),
                                  std::round(placementPos.z));
        float currentFrame=glfwGetTime();
        deltaTime=currentFrame-lastFrame;
        lastFrame=currentFrame;
        keyboard.Update(window);
        mouse.Update(window);

        // Movement
        float speed=5.0f*deltaTime;
        if(keyboard.curr_keys[GLFW_KEY_LEFT_SHIFT]){speed *= 2;}

        Vec3 right = camera.front().cross(Vec3(0,1,0)).normalize();

        Vec3 forwardDir = camera.front();
        forward.y = 0;                 // ignore vertical
        forward = forward.normalized(); // now magnitude is 1

        Vec3 rightDir = right;
        rightDir.y = 0;                // ignore vertical
        rightDir = rightDir.normalized();

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.pos = camera.pos + forward * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.pos = camera.pos - forward * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.pos = camera.pos - rightDir * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.pos = camera.pos + rightDir * speed;



        if(glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS) camera.pos.y += speed;
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) break;
        if(!keyboard.prev_keys['F'] && keyboard.curr_keys['F']) wireframeMode = !wireframeMode;
        
        if(false){
            for(Cube &billy: cubes){
                billy.pos.x += 1 * deltaTime;
                billy.pos.z = sin(billy.pos.x);
                billy.pos.y = cos(billy.pos.x);
            }
        }

        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_LEFT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_LEFT]){
            Vec3 placePos;
            if(target){
                placePos = target->pos + hitNormal; // Place on the face you're looking at
                placePos.x = std::round(placePos.x);
                placePos.y = std::round(placePos.y);
                placePos.z = std::round(placePos.z);
            } else {
                placePos = camera.pos + camera.front() * 7.5f;
                placePos.x = std::round(placePos.x);
                placePos.y = std::round(placePos.y);
                placePos.z = std::round(placePos.z);
            }

            // Only place if empty
            bool canPlace = true;
            for(auto &c: cubes){
                if(c.pos.x == placePos.x && c.pos.y == placePos.y && c.pos.z == placePos.z){
                    canPlace = false; break;
                }
            }

            if(canPlace){
                Cube c;
                c.pos = placePos;
                c.rot = Vec3(0,0,0);
                c.color = Vec3((rand()%100)/100.0f,(rand()%100)/100.0f,(rand()%100)/100.0f);
                cubes.push_back(c);
            }
        }

        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_RIGHT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_RIGHT]){
            Vec3 hitPos, hitNormal;
            if(target){
                auto it = std::find_if(cubes.begin(), cubes.end(), [&](const Cube& c){ return &c == target; });
                if(it != cubes.end()) cubes.erase(it);
            }
        }

        // === FIRST PASS: Render to framebuffer ===
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glViewport(0, 0, screenWidth, screenHeight);
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glEnable(GL_DEPTH_TEST);

        GLuint loc = glGetUniformLocation(shaderProgram,"uMVP");
        GLuint colorLoc = glGetUniformLocation(shaderProgram,"uColor");

        for(auto& c: cubes){
            Mat4 model = Mat4::identity();
            model = multiply(model, model.translate(c.pos));
            model = multiply(model, model.rotateY(c.rot.y));
            Mat4 view = camera.getViewMatrix();
            Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,(float)screenWidth/(float)screenHeight,0.1f,100.0f);
            Mat4 mvp = multiply(proj, multiply(view, model));
            glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
            glUniform3f(colorLoc,c.color.x,c.color.y,c.color.z);

            glBindVertexArray(VAO);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES,0,36*6);
        }

        glUseProgram(outlineShaderProgram);
        GLuint locOutline = glGetUniformLocation(outlineShaderProgram, "uMVP");
        GLuint colorOutlineLoc = glGetUniformLocation(outlineShaderProgram, "uColor");

        // Model/View/Projection
        Mat4 model = Mat4::identity();
        model = multiply(model, model.translate(placementGuide.pos));
        Mat4 view = camera.getViewMatrix();
        Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f, (float)screenWidth/(float)screenHeight, 0.1f, 100.0f);
        Mat4 mvp = multiply(proj, multiply(view, model));
        glUniformMatrix4fv(locOutline, 1, GL_FALSE, mvp.m);

        // Find underlying cube color
        Vec3 underColor = Vec3(0.0f,0.0f,0.0f);
        for(auto &c: cubes){
            if(c.pos.x == placementGuide.pos.x && c.pos.y == placementGuide.pos.y && c.pos.z == placementGuide.pos.z){
                underColor = c.color;
                break;
            }
        }
        glUniform3f(colorOutlineLoc, underColor.x, underColor.y, underColor.z);

        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36*6);

        // === SECOND PASS: Render to screen with crosshair ===
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        // Render the scene again (or copy from framebuffer - but for simplicity, re-render)
        glUseProgram(shaderProgram);
        glEnable(GL_DEPTH_TEST);

        for(auto& c: cubes){
            Mat4 model = Mat4::identity();
            model = multiply(model, model.translate(c.pos));
            model = multiply(model, model.rotateY(c.rot.y));
            Mat4 view = camera.getViewMatrix();
            Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,(float)screenWidth/(float)screenHeight,0.1f,100.0f);
            Mat4 mvp = multiply(proj, multiply(view, model));
            glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
            glUniform3f(colorLoc,c.color.x,c.color.y,c.color.z);

            glBindVertexArray(VAO);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES,0,36*6);
        }

        glUseProgram(outlineShaderProgram);
        model = Mat4::identity();
        model = multiply(model, model.translate(placementGuide.pos));
        view = camera.getViewMatrix();
        proj = Mat4::perspective(45.0f*M_PI/180.0f, (float)screenWidth/(float)screenHeight, 0.1f, 100.0f);
        mvp = multiply(proj, multiply(view, model));
        glUniformMatrix4fv(locOutline, 1, GL_FALSE, mvp.m);
        glUniform3f(colorOutlineLoc, underColor.x, underColor.y, underColor.z);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 36*6);

        // === Render crosshair ===
        glDisable(GL_DEPTH_TEST);
        glUseProgram(crosshairShaderProgram);
        
        // Bind the framebuffer texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glUniform1i(glGetUniformLocation(crosshairShaderProgram, "uFramebuffer"), 0);
        glUniform2f(glGetUniformLocation(crosshairShaderProgram, "uScreenSize"), (float)screenWidth, (float)screenHeight);
        
        glBindVertexArray(crosshairVAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 4);
        
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteVertexArrays(1,&crosshairVAO);
    glDeleteBuffers(1,&crosshairVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(outlineShaderProgram);
    glDeleteProgram(crosshairShaderProgram);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
    glfwTerminate();
    return 0;
}
