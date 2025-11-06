#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "../engine-thingy/cpp-engine.hpp"
#include <unordered_map>
#include <filesystem>
#include <fstream>
namespace fs = std::filesystem;

const int CHUNK_SIZE = 16;
const int RENDER_DISTANCE = 3;
const int HOTBAR_SLOTS = 9;

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
uniform bool uHighlight;
float edgeFactor() {
    vec3 d = fwidth(vBary);
    vec3 a3 = smoothstep(vec3(0.0), d*1.5, vBary);
    return min(min(a3.x,a3.y),a3.z);
}
void main() {
    float factor = edgeFactor();
    if(uHighlight) {
        vec3 outlineColor = vec3(1.0, 1.0, 1.0);
        vec3 brightColor = uColor * 1.5;
        vec3 color = mix(outlineColor, brightColor, factor);
        FragColor = vec4(color, 1.0);
    } else {
        vec3 outlineColor = vec3(0.0,0.0,0.0);
        vec3 color = mix(outlineColor, uColor, factor);
        FragColor = vec4(color, 1.0);
    }
}
)";

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
    vec2 screenCoord = gl_FragCoord.xy / uScreenSize;
    vec3 backgroundColor = texture(uFramebuffer, screenCoord).rgb;
    vec3 invertedColor = vec3(1.0) - backgroundColor;
    float brightness = dot(backgroundColor, vec3(0.299, 0.587, 0.114));
    if (brightness > 0.4 && brightness < 0.6) {
        invertedColor = brightness > 0.5 ? vec3(0.0) : vec3(1.0);
    }
    FragColor = vec4(invertedColor, 1.0);
}
)";

// UI shader for hotbar
const char* uiVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
uniform mat4 uProjection;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
)";

const char* uiFragmentShader = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform vec3 uColor;
uniform bool uIsSelected;
uniform bool uIsBorder;
void main() {
    if(uIsBorder) {
        FragColor = vec4(1.0, 1.0, 1.0, 1.0);
    } else if(uIsSelected) {
        FragColor = vec4(uColor, 1.0);
    } else {
        FragColor = vec4(uColor * 0.7, 1.0);
    }
}
)";

// --------------------
// Cube data
// --------------------
float cubeVertices[] = {
    -0.5f,-0.5f,0.5f, 1,0,0,  0.5f,-0.5f,0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
     0.5f,0.5f,0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0, -0.5f,-0.5f,0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,-0.5f, 0,1,0,  0.5f,-0.5f,-0.5f, 0,0,1,
     0.5f,0.5f,-0.5f, 1,0,0, -0.5f,-0.5f,-0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0, -0.5f,0.5f,-0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0, -0.5f,-0.5f,0.5f, 0,1,0, -0.5f,0.5f,0.5f, 0,0,1,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,-0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
     0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,0.5f,0.5f, 0,1,0,  0.5f,-0.5f,0.5f, 0,0,1,
    -0.5f,0.5f,-0.5f, 1,0,0,  0.5f,0.5f,0.5f, 0,1,0,  0.5f,0.5f,-0.5f, 0,0,1,
    -0.5f,0.5f,-0.5f, 1,0,0, -0.5f,0.5f,0.5f, 0,1,0,  0.5f,0.5f,0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,-0.5f, 0,1,0,  0.5f,-0.5f,0.5f, 0,0,1,
    -0.5f,-0.5f,-0.5f, 1,0,0,  0.5f,-0.5f,0.5f, 0,1,0, -0.5f,-0.5f,0.5f, 0,0,1
};

float crosshairVertices[] = {
    -0.02f,  0.0f,
     0.02f,  0.0f,
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
int selectedHotbarSlot = 0;

// Hotbar colors
Vec3 hotbarColors[HOTBAR_SLOTS] = {
    Vec3(0.8f, 0.2f, 0.2f),  // Red
    Vec3(0.2f, 0.8f, 0.2f),  // Green
    Vec3(0.2f, 0.2f, 0.8f),  // Blue
    Vec3(0.9f, 0.9f, 0.2f),  // Yellow
    Vec3(0.9f, 0.5f, 0.2f),  // Orange
    Vec3(0.6f, 0.2f, 0.8f),  // Purple
    Vec3(0.2f, 0.8f, 0.8f),  // Cyan
    Vec3(0.9f, 0.9f, 0.9f),  // White
    Vec3(0.5f, 0.35f, 0.2f)  // Brown
};

// --------------------
// Structures
// --------------------
struct Cube {
    Vec3 pos;
    Vec3 rot;
    Vec3 color;
    bool do_rotate = 0;
};

struct Chunk {
    Vec3 pos;
    std::vector<Cube> cubes;
    bool dirty = false;
};

std::unordered_map<int64_t, Chunk> loadedChunks;

std::string chunkFilename(int cx, int cz) {
    return "chunks/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".bin";
}

int64_t chunkKey(int cx, int cz) {
    return (int64_t)cx << 32 | (uint32_t)cz;
}

float getHighestBlockY(float x, float z) {
    float highestY = -10000.0f;
    for(auto& [key, chunk] : loadedChunks){
        for(auto& c : chunk.cubes){
            if((int)c.pos.x == (int)std::floor(x) && (int)c.pos.z == (int)std::floor(z)){
                if(c.pos.y > highestY) highestY = c.pos.y;
            }
        }
    }
    return highestY;
}

void saveChunk(const Chunk& chunk) {
    fs::create_directories("chunks");
    std::ofstream ofs(chunkFilename(chunk.pos.x, chunk.pos.z), std::ios::binary);
    size_t n = chunk.cubes.size();
    ofs.write((char*)&n, sizeof(size_t));
    for (auto& c : chunk.cubes) {
        ofs.write((char*)&c.pos, sizeof(Vec3));
        ofs.write((char*)&c.color, sizeof(Vec3));
    }
}

Chunk loadChunk(int cx, int cz) {
    Chunk chunk;
    chunk.pos = Vec3(cx, 0, cz);
    std::ifstream ifs(chunkFilename(cx, cz), std::ios::binary);
    if (ifs) {
        size_t n;
        ifs.read((char*)&n, sizeof(size_t));
        chunk.cubes.resize(n);
        for (size_t i = 0; i < n; i++) {
            ifs.read((char*)&chunk.cubes[i].pos, sizeof(Vec3));
            ifs.read((char*)&chunk.cubes[i].color, sizeof(Vec3));
        }
    } else {
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                Cube c;
                c.pos = Vec3(cx * CHUNK_SIZE + x, 0, cz * CHUNK_SIZE + z);
                c.color = Vec3(0.3f + (rand()%70)/100.0f, 0.3f + (rand()%70)/100.0f, 0.3f + (rand()%70)/100.0f);
                chunk.cubes.push_back(c);
            }
        }
    }
    return chunk;
}

void updateLoadedChunks(const Vec3& cameraPos) {
    int camChunkX = (int)std::floor(cameraPos.x / CHUNK_SIZE);
    int camChunkZ = (int)std::floor(cameraPos.z / CHUNK_SIZE);
    for (int dx = -RENDER_DISTANCE; dx <= RENDER_DISTANCE; dx++) {
        for (int dz = -RENDER_DISTANCE; dz <= RENDER_DISTANCE; dz++) {
            int cx = camChunkX + dx;
            int cz = camChunkZ + dz;
            int64_t key = chunkKey(cx, cz);
            if (loadedChunks.find(key) == loadedChunks.end()) {
                loadedChunks[key] = loadChunk(cx, cz);
            }
        }
    }
    std::vector<int64_t> toUnload;
    for (auto& [key, chunk] : loadedChunks) {
        int cx = chunk.pos.x, cz = chunk.pos.z;
        if (abs(cx - camChunkX) > RENDER_DISTANCE || abs(cz - camChunkZ) > RENDER_DISTANCE) {
            if (chunk.dirty) saveChunk(chunk);
            toUnload.push_back(key);
        }
    }
    for (auto key : toUnload) loadedChunks.erase(key);
}

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
    for(auto& [key, chunk] : loadedChunks){
        for(auto& c : chunk.cubes){
            float t;
            if(rayIntersectsCube(rayOrigin, rayDir, c.pos, t)){
                if(t < closestT){
                    closestT = t;
                    closest = &c;
                }
            }
        }
    }
    if(closest){
        hitPos = rayOrigin + rayDir * closestT;
        Vec3 local = hitPos - closest->pos;
        float absX = std::abs(local.x), absY = std::abs(local.y), absZ = std::abs(local.z);
        if(absX > absY && absX > absZ) hitNormal = Vec3(local.x>0?1:-1,0,0);
        else if(absY > absX && absY > absZ) hitNormal = Vec3(0,local.y>0?1:-1,0);
        else hitNormal = Vec3(0,0,local.z>0?1:-1);
        return closest;
    }
    return nullptr;
}

Vec3 calculatePlacementPosition(const Vec3& hitPos, const Vec3& hitNormal) {
    Vec3 placePos = hitPos + hitNormal * 0.5f;
    placePos.x = std::floor(placePos.x + 0.5f);
    placePos.y = std::floor(placePos.y + 0.5f);
    placePos.z = std::floor(placePos.z + 0.5f);
    return placePos;
}

bool isPositionOccupied(const Vec3& pos) {
    for(auto& [key, chunk] : loadedChunks){
        for(auto& c : chunk.cubes){
            if(c.pos.x == pos.x && c.pos.y == pos.y && c.pos.z == pos.z)
                return true;
        }
    }
    return false;
}

// Function to create a quad for UI elements
void createQuad(float x, float y, float width, float height, float* vertices) {
    // Triangle 1
    vertices[0] = x;           vertices[1] = y;            vertices[2] = 0.0f; vertices[3] = 0.0f;
    vertices[4] = x + width;   vertices[5] = y;            vertices[6] = 1.0f; vertices[7] = 0.0f;
    vertices[8] = x + width;   vertices[9] = y + height;   vertices[10] = 1.0f; vertices[11] = 1.0f;
    // Triangle 2
    vertices[12] = x;          vertices[13] = y;           vertices[14] = 0.0f; vertices[15] = 0.0f;
    vertices[16] = x + width;  vertices[17] = y + height;  vertices[18] = 1.0f; vertices[19] = 1.0f;
    vertices[20] = x;          vertices[21] = y + height;  vertices[22] = 0.0f; vertices[23] = 1.0f;
}

// --------------------
// Main
// --------------------
int main(){
    if(!glfwInit()) return -1;
    camera.pos = Vec3(0,0,0);
    updateLoadedChunks(camera.pos);
    
    camera.pos.y = getHighestBlockY(camera.pos.x, camera.pos.z) + 1.0f;
    camera.pos.y += 1;
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    
    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Mini FPS Game", monitor, nullptr);
    if(!window){ glfwTerminate(); return -1; }
    
    int screenWidth = mode->width;
    int screenHeight = mode->height;
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window,camera.mouse_callback);
    glfwSetInputMode(window,GLFW_CURSOR,GLFW_CURSOR_DISABLED);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return -1;
    }

    // Compile main shader
    GLuint vs=glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs,1,&vertexShaderSource,nullptr); glCompileShader(vs);
    GLuint fs=glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs,1,&fragmentShaderSource,nullptr); glCompileShader(fs);
    GLuint shaderProgram=glCreateProgram();
    glAttachShader(shaderProgram,vs); glAttachShader(shaderProgram,fs);
    glLinkProgram(shaderProgram);
    glDeleteShader(vs); glDeleteShader(fs);

    // Setup cube VAO
    GLuint VAO,VBO;
    glGenVertexArrays(1,&VAO);
    glGenBuffers(1,&VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER,VBO);
    glBufferData(GL_ARRAY_BUFFER,sizeof(cubeVertices),cubeVertices,GL_STATIC_DRAW);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glEnable(GL_DEPTH_TEST);

    // Compile crosshair shader
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

    // Compile UI shader
    GLuint vsUI = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsUI, 1, &uiVertexShader, nullptr);
    glCompileShader(vsUI);
    GLuint fsUI = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsUI, 1, &uiFragmentShader, nullptr);
    glCompileShader(fsUI);
    GLuint uiShaderProgram = glCreateProgram();
    glAttachShader(uiShaderProgram, vsUI);
    glAttachShader(uiShaderProgram, fsUI);
    glLinkProgram(uiShaderProgram);
    glDeleteShader(vsUI);
    glDeleteShader(fsUI);

    // Setup UI VAO for hotbar
    GLuint uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Setup framebuffer
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

    // Main loop
    while(!glfwWindowShouldClose(window)){
        float currentFrame=glfwGetTime();
        deltaTime=currentFrame-lastFrame;
        lastFrame=currentFrame;
        keyboard.Update(window);
        mouse.Update(window);

        // Hotbar slot selection
        for(int i = 0; i < HOTBAR_SLOTS; i++) {
            if(!keyboard.prev_keys[GLFW_KEY_1 + i] && keyboard.curr_keys[GLFW_KEY_1 + i]) {
                selectedHotbarSlot = i;
            }
        }

        // Get target cube
        Vec3 hitPos, hitNormal;
        Cube* targetCube = getCubeUnderCursor(camera.pos, camera.front(), 7.5f, hitPos, hitNormal);

        // Movement
        float speed=5.0f*deltaTime;
        if(keyboard.curr_keys[GLFW_KEY_LEFT_CONTROL]){speed *= 2;}
        Vec3 right = camera.front().cross(Vec3(0,1,0)).normalize();
        Vec3 forwardDir = camera.front();
        forwardDir.y = 0;
        forwardDir = forwardDir.normalize();
        Vec3 rightDir = right;
        rightDir.y = 0;
        rightDir = rightDir.normalize();
        
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.pos = camera.pos + forwardDir * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.pos = camera.pos - forwardDir * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.pos = camera.pos - rightDir * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.pos = camera.pos + rightDir * speed;
        if(glfwGetKey(window,GLFW_KEY_SPACE)==GLFW_PRESS) camera.pos.y += speed; 
        if(glfwGetKey(window,GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS) camera.pos.y -= speed;
        if(glfwGetKey(window,GLFW_KEY_ESCAPE)==GLFW_PRESS) break;
        if(!keyboard.prev_keys['F'] && keyboard.curr_keys['F']) wireframeMode = !wireframeMode;
        
        updateLoadedChunks(camera.pos);

        // Place cube with selected hotbar color
        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_LEFT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_LEFT]){
            if(targetCube){
                Vec3 placePos = calculatePlacementPosition(hitPos, hitNormal);
                if(!isPositionOccupied(placePos)){
                    int cx = (int)std::floor(placePos.x / CHUNK_SIZE);
                    int cz = (int)std::floor(placePos.z / CHUNK_SIZE);
                    int64_t key = chunkKey(cx, cz);
                    Chunk& chunk = loadedChunks[key];
                    Cube c;
                    c.do_rotate = 1;
                    c.pos = placePos;
                    c.rot = Vec3(0,0,0);
                    c.color = hotbarColors[selectedHotbarSlot];
                    chunk.cubes.push_back(c);
                    chunk.dirty = true;
                }
            }
        }

        // Break cube
        if(!mouse.prev_buttons[GLFW_MOUSE_BUTTON_RIGHT] && mouse.curr_buttons[GLFW_MOUSE_BUTTON_RIGHT]){
            if(targetCube){
                int cx = (int)std::floor(targetCube->pos.x / CHUNK_SIZE);
                int cz = (int)std::floor(targetCube->pos.z / CHUNK_SIZE);
                int64_t key = chunkKey(cx, cz);
                Chunk& chunk = loadedChunks[key];
                auto it = std::find_if(chunk.cubes.begin(), chunk.cubes.end(),
                                       [&](const Cube& c){ return &c == targetCube; });
                if(it != chunk.cubes.end()){
                    chunk.cubes.erase(it);
                    chunk.dirty = true;
                }
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
        GLuint highlightLoc = glGetUniformLocation(shaderProgram,"uHighlight");

        for (auto& [key, chunk] : loadedChunks) {
            for (auto& c : chunk.cubes) {
                Mat4 model = Mat4::identity();
                model = multiply(model, model.translate(c.pos));
                Mat4 view = camera.getViewMatrix();
                Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,(float)screenWidth/(float)screenHeight,0.1f,100.0f);
                Mat4 mvp = multiply(proj, multiply(view, model));
                glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
                glUniform3f(colorLoc,c.color.x,c.color.y,c.color.z);
                bool isHighlighted = (targetCube == &c);
                glUniform1i(highlightLoc, isHighlighted);
                glBindVertexArray(VAO);
                glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
                glDrawArrays(GL_TRIANGLES,0,36);
            }
        }

        // === SECOND PASS: Render to screen ===
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glClearColor(0.1f,0.1f,0.1f,1.0f);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        
        // Render the scene again
        glUseProgram(shaderProgram);
        glEnable(GL_DEPTH_TEST);
        
        for (auto& [key, chunk] : loadedChunks) {
            for (auto& c : chunk.cubes) {
                Mat4 model = Mat4::identity();
                model = multiply(model, model.translate(c.pos));
                Mat4 view = camera.getViewMatrix();
                Mat4 proj = Mat4::perspective(45.0f*M_PI/180.0f,(float)screenWidth/(float)screenHeight,0.1f,100.0f);
                Mat4 mvp = multiply(proj, multiply(view, model));
                glUniformMatrix4fv(loc,1,GL_FALSE,mvp.m);
                glUniform3f(colorLoc,c.color.x,c.color.y,c.color.z);
                bool isHighlighted = (targetCube == &c);
                glUniform1i(highlightLoc, isHighlighted);
                glBindVertexArray(VAO);
                glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
                glDrawArrays(GL_TRIANGLES,0,36);
            }
        }

        // === Render crosshair ===
        glDisable(GL_DEPTH_TEST);
        glUseProgram(crosshairShaderProgram);
        
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glUniform1i(glGetUniformLocation(crosshairShaderProgram, "uFramebuffer"), 0);
        glUniform2f(glGetUniformLocation(crosshairShaderProgram, "uScreenSize"), (float)screenWidth, (float)screenHeight);
        
        glBindVertexArray(crosshairVAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 4);

        // === Render hotbar ===
        glUseProgram(uiShaderProgram);
        
        // Create orthographic projection for UI
        Mat4 orthoProj = Mat4::identity();
        orthoProj.m[0] = 2.0f / screenWidth;
        orthoProj.m[5] = 2.0f / screenHeight;
        orthoProj.m[10] = -1.0f;
        orthoProj.m[12] = -1.0f;
        orthoProj.m[13] = -1.0f;
        
        GLuint projLoc = glGetUniformLocation(uiShaderProgram, "uProjection");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, orthoProj.m);
        
        float slotSize = 60.0f;
        float slotSpacing = 10.0f;
        float totalWidth = HOTBAR_SLOTS * slotSize + (HOTBAR_SLOTS - 1) * slotSpacing;
        float startX = (screenWidth - totalWidth) / 2.0f;
        float startY = 20.0f;
        
        glBindVertexArray(uiVAO);
        
        for(int i = 0; i < HOTBAR_SLOTS; i++) {
            float x = startX + i * (slotSize + slotSpacing);
            float y = startY;
            
            // Draw border (white for selected, darker for others)
            float borderSize = 4.0f;
            float quadVerts[24];
            createQuad(x - borderSize, y - borderSize, slotSize + 2 * borderSize, slotSize + 2 * borderSize, quadVerts);
            
            glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);
            
            glUniform3f(glGetUniformLocation(uiShaderProgram, "uColor"), 1.0f, 1.0f, 1.0f);
            glUniform1i(glGetUniformLocation(uiShaderProgram, "uIsBorder"), 1);
            glUniform1i(glGetUniformLocation(uiShaderProgram, "uIsSelected"), i == selectedHotbarSlot);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            
            // Draw slot with color
            createQuad(x, y, slotSize, slotSize, quadVerts);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quadVerts), quadVerts);
            
            Vec3 color = hotbarColors[i];
            glUniform3f(glGetUniformLocation(uiShaderProgram, "uColor"), color.x, color.y, color.z);
            glUniform1i(glGetUniformLocation(uiShaderProgram, "uIsBorder"), 0);
            glUniform1i(glGetUniformLocation(uiShaderProgram, "uIsSelected"), i == selectedHotbarSlot);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        
        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    // Save chunks before exit
    for (auto& [key, chunk] : loadedChunks) {
        if (chunk.dirty) saveChunk(chunk);
    }
    
    glDeleteVertexArrays(1,&VAO);
    glDeleteBuffers(1,&VBO);
    glDeleteVertexArrays(1,&crosshairVAO);
    glDeleteBuffers(1,&crosshairVBO);
    glDeleteVertexArrays(1,&uiVAO);
    glDeleteBuffers(1,&uiVBO);
    glDeleteProgram(shaderProgram);
    glDeleteProgram(crosshairShaderProgram);
    glDeleteProgram(uiShaderProgram);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &colorTexture);
    glDeleteRenderbuffers(1, &depthBuffer);
    glfwTerminate();
    return 0;
}
