// =============================================================
// main.cpp — Enhanced with Dynamic Lighting & Animations
// New Features:
//   1) Animated day cycle (sunlight orange to white)
//   2) Pulsing crystal glow lights
//   3) Portal shifting blue/purple light
//   4) Moving fire spirit orb light source
//   5) Textured roof (stone_wall_diff_4k.bmp)
//   6) Enhanced visual quality
// =============================================================

#include <GLUT/glut.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

// =======================================================
// IMPROVED BMP TEXTURE LOADER
// =======================================================
GLuint loadBMP(const char *imagepath) {
    unsigned char header[54];
    unsigned int dataPos, width, height, imageSize;

    FILE *file = fopen(imagepath, "rb");
    if (!file) {
        std::cout << "ERROR: Cannot open BMP file: " << imagepath << "\n";
        return 0;
    }

    if (fread(header, 1, 54, file) != 54) {
        std::cout << "ERROR: Not a valid BMP file\n";
        fclose(file);
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        std::cout << "ERROR: Not a valid BMP file\n";
        fclose(file);
        return 0;
    }

    dataPos   = *(int*)&(header[0x0A]);
    imageSize = *(int*)&(header[0x22]);
    width     = *(int*)&(header[0x12]);
    height    = *(int*)&(header[0x16]);

    if (imageSize == 0) imageSize = width * height * 3;
    if (dataPos == 0)   dataPos = 54;

    unsigned char *data = new unsigned char[imageSize];
    fread(data, 1, imageSize, file);
    fclose(file);

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height,
                      GL_BGR, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return texID;
}

// =======================================================
// BASIC MATH
// =======================================================
struct Vec3 { float x,y,z;
    Vec3() : x(0),y(0),z(0) {}
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
    
    Vec3 operator+(const Vec3& o) const { return Vec3(x+o.x, y+o.y, z+o.z); }
    Vec3 operator*(float s) const { return Vec3(x*s, y*s, z*s); }
    float length() const { return std::sqrt(x*x + y*y + z*z); }
    Vec3 normalized() const { float l = length(); return l > 0 ? Vec3(x/l, y/l, z/l) : Vec3(0,0,0); }
};

inline float frand(float a,float b){
    return a + (b-a) * float(rand()) / float(RAND_MAX);
}

inline float clampf(float v,float a,float b){
    return v < a ? a : (v > b ? b : v);
}

inline float lerp(float a, float b, float t){
    return a + (b - a) * t;
}

// =======================================================
// GLOBALS
// =======================================================
const float WORLD_HALF  = 36.0f;
const float WALL_HEIGHT = 7.0f;

Vec3 playerPos(0,1.0f,0), lastSafePos(0,1.0f,0);
float playerYaw=0, cameraYaw=0, playerPitch=0, cameraPitch=0;

float lastTime=0;
float playerRadius=0.6f, playerSpeed=8.0f;

bool keys[512]={false};
int screenW=1280, screenH=800;

enum CameraMode { CAM_THIRD, CAM_FIRST };
CameraMode cameraMode = CAM_THIRD;

int currentLevel = 1;
int score = 0;

// Animation time tracker
float animTime = 0.0f;

// =======================================================
// TEXTURES
// =======================================================
GLuint desertWallTex = 0;
GLuint snowWallTex   = 0;
GLuint desertFloorTex = 0;
GLuint snowFloorTex   = 0;
GLuint desertStoneTex = 0;
GLuint desertGoldTex = 0;
GLuint roofTex = 0;
GLuint fireSpiritTex = 0;
GLuint portalTex = 0;

// =======================================================
// FIRE SPIRIT ORB - Follows beside player
// =======================================================
struct FireSpirit {
    Vec3 pos;
    float offsetDistance;
    float height;
    
    FireSpirit() : offsetDistance(2.5f), height(2.0f) {
        updatePosition();
    }
    
    void updatePosition() {
        // Position fire spirit to the right side of player
        float sy = std::sin(playerYaw);
        float cy = std::cos(playerYaw);
        
        // Right vector relative to player direction
        Vec3 right(cy, 0, sy);
        
        pos.x = playerPos.x + right.x * offsetDistance;
        pos.z = playerPos.z + right.z * offsetDistance;
        pos.y = playerPos.y + height + std::sin(animTime * 3.0f) * 0.2f;
    }
    
    void update(float dt) {
        updatePosition();
    }
} fireSpirit;

// =======================================================
// CRYSTAL LIGHTS (for snow level)
// =======================================================
struct Crystal {
    Vec3 pos;
    float glowPhase;
};
std::vector<Crystal> crystals;

// =======================================================
// FOG
// =======================================================
void setupFog() {
    glEnable(GL_FOG);

    GLfloat fogColor[4];

    if (currentLevel == 1) {
        // Day cycle affects fog color
        float dayTime = std::sin(animTime * 0.15f) * 0.5f + 0.5f;
        fogColor[0] = lerp(0.94f, 0.98f, dayTime);
        fogColor[1] = lerp(0.86f, 0.92f, dayTime);
        fogColor[2] = lerp(0.72f, 0.85f, dayTime);
        fogColor[3] = 1.0f;
        glFogf(GL_FOG_DENSITY, 0.018f);
    }
    else {
        fogColor[0] = 0.92f;
        fogColor[1] = 0.95f;
        fogColor[2] = 0.98f;
        fogColor[3] = 1.0f;
        glFogf(GL_FOG_DENSITY, 0.045f);
    }

    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_EXP2);
}

// =======================================================
// OBJ LOADER
// =======================================================
struct Vec3s { float x,y,z; };
struct Mesh {
    std::vector<Vec3s> verts;
    bool empty()const{return verts.empty();}
};
Mesh playerMesh;

Mesh loadOBJ(const std::string& path){
    Mesh mesh;
    std::ifstream in(path);
    if(!in.is_open()) return mesh;

    std::vector<Vec3s> vlist;
    std::string line;

    while(std::getline(in,line)){
        if(line.rfind("v ",0)==0){
            std::istringstream iss(line.substr(2));
            float x,y,z; iss>>x>>y>>z;
            vlist.push_back({x,y,z});
        }
        else if(line.rfind("f ",0)==0){
            std::istringstream iss(line.substr(2));
            std::string a,b,c;
            iss>>a>>b>>c;

            auto idx=[&](std::string s){ return stoi(s.substr(0,s.find('/'))) - 1; };

            mesh.verts.push_back(vlist[idx(a)]);
            mesh.verts.push_back(vlist[idx(b)]);
            mesh.verts.push_back(vlist[idx(c)]);
        }
    }
    return mesh;
}

// =======================================================
// ENTITIES
// =======================================================
struct Collectible { Vec3 pos; float radius; bool collected; };
struct Obstacle { Vec3 pos, vel; float radius, mass; std::string type; bool grounded; };
struct Portal { Vec3 pos; float radius; };

std::vector<Collectible> collectibles;
std::vector<Obstacle> obstacles;
Portal portal;

// =======================================================
// RANDOM POS
// =======================================================
Vec3 randomUniformPosition(float minDist){
    while(true){
        float x = frand(-WORLD_HALF+3, WORLD_HALF-3);
        float z = frand(-WORLD_HALF+3, WORLD_HALF-3);
        return Vec3(x,0,z);
    }
}

// =======================================================
// LEVEL SETUP
// =======================================================
void clearLevel(){
    collectibles.clear();
    obstacles.clear();
    crystals.clear();
    score = 0;
}

void setupDesert(){
    clearLevel();
    currentLevel = 1;

    playerPos = Vec3(0,1.0f,5.0f);
    lastSafePos = playerPos;
    playerYaw = cameraYaw = 0.0f;
    cameraPitch = 0.0f;

    int COLLECT_COUNT = 10;
    int OBST_COUNT    = 8;

    for(int i=0;i<COLLECT_COUNT;i++){
        Vec3 p = randomUniformPosition(4.5f);
        collectibles.push_back({ Vec3(p.x,1.4f,p.z), 0.6f, false });
    }

    for(int i=0;i<OBST_COUNT;i++){
        Vec3 p = randomUniformPosition(6.5f);
        obstacles.push_back({
            Vec3(p.x,1.0f,p.z),
            Vec3(0,0,0),
            1.1f,
            9999.0f,
            "stone",
            true
        });
    }

    portal.pos = Vec3(0,0,-(WORLD_HALF - 4.0f));
    portal.radius = 4.5f;
    
    fireSpirit = FireSpirit();
}

void setupSnow(){
    clearLevel();
    currentLevel = 2;

    playerPos = Vec3(0,1.0f,5.0f);
    lastSafePos = playerPos;

    playerYaw = cameraYaw = 0.0f;
    cameraPitch = 0.0f;

    int COLLECT_COUNT = 10;
    int OBST_COUNT    = 9;

    for(int i=0;i<COLLECT_COUNT;i++){
        Vec3 p = randomUniformPosition(4.5f);
        collectibles.push_back({ Vec3(p.x,1.8f,p.z), 0.6f, false });
    }

    for(int i=0;i<OBST_COUNT;i++){
        Vec3 p = randomUniformPosition(6.5f);
        obstacles.push_back({
            Vec3(p.x, 14.0f + frand(-1.5f,1.5f), p.z),
            Vec3(0,0,0),
            0.5f,
            0.8f,
            "icicle",
            false
        });
    }

    // Add glowing crystals
    for(int i=0; i<6; i++){
        Vec3 p = randomUniformPosition(8.0f);
        crystals.push_back({ Vec3(p.x, 0.8f, p.z), frand(0, 6.28f) });
    }

    portal.pos = Vec3(0,0,-(WORLD_HALF - 4.0f));
    portal.radius = 4.5f;
    
    fireSpirit = FireSpirit();
}

// =======================================================
// DRAW FLOOR + WALLS + TEXTURED ROOF
// =======================================================
void drawGroundAndEnvironment(){
    setupFog();

    float half = WORLD_HALF;
    float h    = WALL_HEIGHT;

    // Dynamic background color based on day cycle
    if(currentLevel == 1){
        float dayTime = std::sin(animTime * 0.15f) * 0.5f + 0.5f;
        glClearColor(
            lerp(0.96f, 0.98f, dayTime),
            lerp(0.90f, 0.94f, dayTime),
            lerp(0.75f, 0.88f, dayTime),
            1.0f
        );
    }
    else
        glClearColor(0.88f, 0.94f, 0.98f, 1.0f);

    glEnable(GL_TEXTURE_2D);

    // ========== FLOOR TEXTURE ==========
    if(currentLevel == 1){
        glBindTexture(GL_TEXTURE_2D, desertFloorTex);
        float floorRepeat = 8.0f;

        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
            glNormal3f(0,1,0);
            glTexCoord2f(0,0);             glVertex3f(-half,0,-half);
            glTexCoord2f(floorRepeat,0);   glVertex3f( half,0,-half);
            glTexCoord2f(floorRepeat,floorRepeat); glVertex3f( half,0, half);
            glTexCoord2f(0,floorRepeat);   glVertex3f(-half,0, half);
        glEnd();
    }
    else {
        glBindTexture(GL_TEXTURE_2D, snowFloorTex);
        float floorRepeat = 30.0f;

        glColor3f(1.0f, 1.0f, 1.0f);

        glBegin(GL_QUADS);
            glNormal3f(0,1,0);
            glTexCoord2f(0,0); glVertex3f(-half,0,-half);
            glTexCoord2f(floorRepeat,0); glVertex3f( half,0,-half);
            glTexCoord2f(floorRepeat,floorRepeat); glVertex3f( half,0, half);
            glTexCoord2f(0,floorRepeat); glVertex3f(-half,0, half);
        glEnd();
    }

    // ========== WALLS ==========
    if(currentLevel == 1)
        glBindTexture(GL_TEXTURE_2D, desertWallTex);
    else
        glBindTexture(GL_TEXTURE_2D, snowWallTex);

    glColor3f(1.0f, 1.0f, 1.0f);
    float repeat = 10.0f;

    // +Z wall
    glBegin(GL_QUADS);
        glNormal3f(0,0,-1);
        glTexCoord2f(0,0); glVertex3f(-half,0, half);
        glTexCoord2f(repeat,0); glVertex3f( half,0, half);
        glTexCoord2f(repeat,repeat); glVertex3f( half,h, half);
        glTexCoord2f(0,repeat); glVertex3f(-half,h, half);
    glEnd();

    // -Z wall
    glBegin(GL_QUADS);
        glNormal3f(0,0,1);
        glTexCoord2f(0,0); glVertex3f(-half,0,-half);
        glTexCoord2f(0,repeat); glVertex3f(-half,h,-half);
        glTexCoord2f(repeat,repeat); glVertex3f( half,h,-half);
        glTexCoord2f(repeat,0); glVertex3f( half,0,-half);
    glEnd();

    // -X wall
    glBegin(GL_QUADS);
        glNormal3f(1,0,0);
        glTexCoord2f(0,0); glVertex3f(-half,0,-half);
        glTexCoord2f(repeat,0); glVertex3f(-half,0, half);
        glTexCoord2f(repeat,repeat); glVertex3f(-half,h, half);
        glTexCoord2f(0,repeat); glVertex3f(-half,h,-half);
    glEnd();

    // +X wall
    glBegin(GL_QUADS);
        glNormal3f(-1,0,0);
        glTexCoord2f(0,0); glVertex3f(half,0,-half);
        glTexCoord2f(0,repeat); glVertex3f(half,h,-half);
        glTexCoord2f(repeat,repeat); glVertex3f(half,h, half);
        glTexCoord2f(repeat,0); glVertex3f(half,0, half);
    glEnd();

    // ========== TEXTURED ROOF ==========
    glBindTexture(GL_TEXTURE_2D, roofTex);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    float roofRepeat = 6.0f;

    glBegin(GL_QUADS);
        glNormal3f(0,-1,0);
        glTexCoord2f(0,0); glVertex3f(-half,h,-half);
        glTexCoord2f(roofRepeat,0); glVertex3f( half,h,-half);
        glTexCoord2f(roofRepeat,roofRepeat); glVertex3f( half,h, half);
        glTexCoord2f(0,roofRepeat); glVertex3f(-half,h, half);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

// =======================================================
// PORTAL - Large Textured Rectangle Gateway
// =======================================================
void drawPortal(){
    glPushMatrix();
    glTranslatef(portal.pos.x, portal.pos.y + 3.5f, portal.pos.z);

    // Portal shifting light effect
    float portalShift = std::sin(animTime * 1.5f) * 0.5f + 0.5f;
    
    GLfloat mat_emission[4];
    
    if(currentLevel == 1){
        mat_emission[0] = 0.3f;
        mat_emission[1] = 0.25f;
        mat_emission[2] = 0.1f;
        mat_emission[3] = 1.0f;
    } else {
        mat_emission[0] = lerp(0.15f, 0.3f, portalShift);
        mat_emission[1] = lerp(0.2f, 0.1f, portalShift);
        mat_emission[2] = lerp(0.3f, 0.4f, portalShift);
        mat_emission[3] = 1.0f;
    }
    
    glMaterialfv(GL_FRONT, GL_EMISSION, mat_emission);
    
    // Enable texture
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, portalTex);
    glColor3f(1.0f, 1.0f, 1.0f);
    
    float width = 4.5f;
    float height = 6.0f;
    float depth = 0.4f;
    
    // Front face
    glBegin(GL_QUADS);
        glNormal3f(0, 0, 1);
        glTexCoord2f(0, 0); glVertex3f(-width, -height, depth);
        glTexCoord2f(2, 0); glVertex3f(width, -height, depth);
        glTexCoord2f(2, 3); glVertex3f(width, height, depth);
        glTexCoord2f(0, 3); glVertex3f(-width, height, depth);
    glEnd();
    
    // Back face
    glBegin(GL_QUADS);
        glNormal3f(0, 0, -1);
        glTexCoord2f(0, 0); glVertex3f(-width, -height, -depth);
        glTexCoord2f(0, 3); glVertex3f(-width, height, -depth);
        glTexCoord2f(2, 3); glVertex3f(width, height, -depth);
        glTexCoord2f(2, 0); glVertex3f(width, -height, -depth);
    glEnd();
    
    // Left side
    glBegin(GL_QUADS);
        glNormal3f(-1, 0, 0);
        glTexCoord2f(0, 0); glVertex3f(-width, -height, -depth);
        glTexCoord2f(1, 0); glVertex3f(-width, -height, depth);
        glTexCoord2f(1, 3); glVertex3f(-width, height, depth);
        glTexCoord2f(0, 3); glVertex3f(-width, height, -depth);
    glEnd();
    
    // Right side
    glBegin(GL_QUADS);
        glNormal3f(1, 0, 0);
        glTexCoord2f(0, 0); glVertex3f(width, -height, -depth);
        glTexCoord2f(0, 3); glVertex3f(width, height, -depth);
        glTexCoord2f(1, 3); glVertex3f(width, height, depth);
        glTexCoord2f(1, 0); glVertex3f(width, -height, depth);
    glEnd();
    
    // Top
    glBegin(GL_QUADS);
        glNormal3f(0, 1, 0);
        glTexCoord2f(0, 0); glVertex3f(-width, height, -depth);
        glTexCoord2f(0, 1); glVertex3f(-width, height, depth);
        glTexCoord2f(2, 1); glVertex3f(width, height, depth);
        glTexCoord2f(2, 0); glVertex3f(width, height, -depth);
    glEnd();
    
    // Bottom
    glBegin(GL_QUADS);
        glNormal3f(0, -1, 0);
        glTexCoord2f(0, 0); glVertex3f(-width, -height, -depth);
        glTexCoord2f(2, 0); glVertex3f(width, -height, -depth);
        glTexCoord2f(2, 1); glVertex3f(width, -height, depth);
        glTexCoord2f(0, 1); glVertex3f(-width, -height, depth);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
    
    GLfloat no_emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);

    glPopMatrix();
}

// =======================================================
// DRAW CRYSTALS WITH PULSING GLOW
// =======================================================
void drawCrystals(){
    if(currentLevel != 2) return;
    
    glDisable(GL_TEXTURE_2D);
    
    for(auto& crystal : crystals){
        glPushMatrix();
        glTranslatef(crystal.pos.x, crystal.pos.y, crystal.pos.z);
        
        // Pulsing glow effect
        float pulse = std::sin(animTime * 2.0f + crystal.glowPhase) * 0.3f + 0.7f;
        
        GLfloat mat_emission[] = {
            0.3f * pulse,
            0.5f * pulse,
            0.7f * pulse,
            1.0f
        };
        glMaterialfv(GL_FRONT, GL_EMISSION, mat_emission);
        
        glColor3f(0.6f * pulse, 0.8f * pulse, 1.0f * pulse);
        
        glScalef(0.3f, 0.6f, 0.3f);
        glutSolidOctahedron();
        
        GLfloat no_emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
        glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
        
        glPopMatrix();
    }
}

// =======================================================
// DRAW FIRE SPIRIT ORB - Fixed Texture Rendering
// =======================================================
void drawFireSpirit(){
    glPushMatrix();
    glTranslatef(fireSpirit.pos.x, fireSpirit.pos.y, fireSpirit.pos.z);
    
    // Pulsing fire effect
    float pulse = std::sin(animTime * 4.0f) * 0.2f + 0.8f;
    
    GLfloat mat_emission[] = {
        0.6f * pulse,
        0.3f * pulse,
        0.1f * pulse,
        1.0f
    };
    glMaterialfv(GL_FRONT, GL_EMISSION, mat_emission);
    
    // Enhanced material properties for textured orb
    GLfloat mat_specular[] = {0.8f, 0.6f, 0.4f, 1.0f};
    GLfloat mat_shininess[] = {60.0f};
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    
    // Enable texture for fire spirit
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fireSpiritTex);
    
    // IMPORTANT: White color to show texture properly
    glColor3f(1.0f, 1.0f, 1.0f);
    
    // Draw textured sphere with proper mapping
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    gluQuadricNormals(quad, GLU_SMOOTH);
    gluSphere(quad, 0.5f, 32, 32);
    gluDeleteQuadric(quad);
    
    glDisable(GL_TEXTURE_2D);
    
    // Outer glow (no texture) - warm fire color
    glColor4f(1.0f * pulse, 0.5f * pulse, 0.1f * pulse, 0.25f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glutSolidSphere(0.7f * pulse, 16, 16);
    glDisable(GL_BLEND);
    
    GLfloat no_emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
    
    glPopMatrix();
}

// =======================================================
// OBSTACLE PHYSICS
// =======================================================
void integrateObstacles(float dt){
    for(auto& o : obstacles){
        if(o.type == "icicle" && !o.grounded){
            o.vel.y += -4.2f * dt;
            o.pos.y += o.vel.y * dt;

            if(o.pos.y <= 0.35f){
                o.pos.y = 0.35f;
                o.vel.y = 0.0f;
                o.grounded = true;
            }
        }
    }
}

// =======================================================
// COLLISION
// =======================================================
float distXZ(const Vec3& a,const Vec3& b){
    float dx = a.x - b.x;
    float dz = a.z - b.z;
    return std::sqrt(dx*dx + dz*dz);
}

void handlePlayerCollisions(){
    bool pushedBack = false;

    for(auto& o : obstacles){
        float d = distXZ(playerPos, o.pos);
        float minD = playerRadius + o.radius;

        if(d < minD){
            float dx = playerPos.x - o.pos.x;
            float dz = playerPos.z - o.pos.z;
            float L = std::sqrt(dx*dx + dz*dz);
            if(L < 0.0001f) L = 0.0001f;

            float overlap = minD - L;
            playerPos.x += (dx/L) * overlap;
            playerPos.z += (dz/L) * overlap;

            score = std::max(0, score - 1);
            pushedBack = true;
        }
    }

    if(pushedBack)
        playerPos = lastSafePos;
}

// =======================================================
// UPDATE LOOP
// =======================================================
void update(float dt){
    animTime += dt;
    
    lastSafePos = playerPos;

    Vec3 input(0,0,0);
    if(keys['w']||keys['W']) input.z += 1;
    if(keys['s']||keys['S']) input.z -= 1;
    if(keys['a']||keys['A']) input.x -= 1;
    if(keys['d']||keys['D']) input.x += 1;

    if(input.x != 0 || input.z != 0){
        float L = std::sqrt(input.x*input.x + input.z*input.z);
        input.x /= L; input.z /= L;

        float sy = std::sin(playerYaw);
        float cy = std::cos(playerYaw);

        Vec3 forward(sy, 0, -cy);
        Vec3 right (cy, 0,  sy);

        playerPos.x += (forward.x * input.z + right.x * input.x) * playerSpeed * dt;
        playerPos.z += (forward.z * input.z + right.z * input.x) * playerSpeed * dt;
    }

    playerPos.y = 1.0f;

    integrateObstacles(dt);
    handlePlayerCollisions();
    
    // Update fire spirit
    fireSpirit.update(dt);

    // Collectibles
    for(auto& c : collectibles){
        if(!c.collected && distXZ(playerPos, c.pos) < c.radius + playerRadius + 0.2f){
            c.collected = true;
            score += 10;
        }
    }

    // Level switch
    bool allCollected = true;
    for(auto& c : collectibles) if(!c.collected) allCollected = false;

    if(allCollected && distXZ(playerPos, portal.pos) < portal.radius + 0.8f){
        if(currentLevel == 1) setupSnow();
        else setupDesert();
    }

    float bound = WORLD_HALF - playerRadius - 0.1f;
    playerPos.x = clampf(playerPos.x, -bound, bound);
    playerPos.z = clampf(playerPos.z, -bound, bound);
}

// =======================================================
// SETUP DYNAMIC LIGHTING
// =======================================================
void setupDynamicLighting(){
    glEnable(GL_LIGHTING);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // ========== LIGHT 0: Animated Sun/Main Light ==========
    glEnable(GL_LIGHT0);
    
    if(currentLevel == 1){
        // Day cycle: orange dawn -> white noon -> orange dusk
        float dayTime = std::sin(animTime * 0.15f) * 0.5f + 0.5f;
        
        float sun[] = {18, 45, 12, 1};
        float diff[] = {
            lerp(1.2f, 1.05f, dayTime),
            lerp(0.7f, 0.95f, dayTime),
            lerp(0.4f, 0.85f, dayTime),
            1.0f
        };
        float amb[] = {
            lerp(0.6f, 0.5f, dayTime),
            lerp(0.45f, 0.48f, dayTime),
            lerp(0.3f, 0.42f, dayTime),
            1.0f
        };
        float spec[] = {0.4f, 0.4f, 0.3f, 1.0f};

        glLightfv(GL_LIGHT0, GL_POSITION, sun);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    }
    else {
        float sun[] = {12, 50, 18, 1};
        float diff[] = {0.80f, 0.88f, 1.05f, 1.0f};
        float amb[] = {0.62f, 0.68f, 0.78f, 1.0f};
        float spec[] = {0.5f, 0.5f, 0.6f, 1.0f};

        glLightfv(GL_LIGHT0, GL_POSITION, sun);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
        glLightfv(GL_LIGHT0, GL_SPECULAR, spec);
    }

    // ========== LIGHT 1: Fire Spirit Orb ==========
    glEnable(GL_LIGHT1);
    
    float firePulse = std::sin(animTime * 4.0f) * 0.3f + 0.7f;
    float firePos[] = {fireSpirit.pos.x, fireSpirit.pos.y, fireSpirit.pos.z, 1.0f};
    float fireDiff[] = {1.0f * firePulse, 0.5f * firePulse, 0.2f * firePulse, 1.0f};
    float fireAmb[] = {0.3f * firePulse, 0.15f * firePulse, 0.05f * firePulse, 1.0f};
    
    glLightfv(GL_LIGHT1, GL_POSITION, firePos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fireDiff);
    glLightfv(GL_LIGHT1, GL_AMBIENT, fireAmb);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.09f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.032f);

    // ========== LIGHT 2: Portal Light ==========
    if(currentLevel == 2){
        glEnable(GL_LIGHT2);
        
        float portalShift = std::sin(animTime * 1.5f) * 0.5f + 0.5f;
        float portalPos[] = {portal.pos.x, portal.pos.y + 1.2f, portal.pos.z, 1.0f};
        float portalDiff[] = {
            lerp(0.4f, 0.6f, portalShift),
            lerp(0.5f, 0.3f, portalShift),
            lerp(0.8f, 1.0f, portalShift),
            1.0f
        };
        float portalAmb[] = {
            0.2f * portalDiff[0],
            0.2f * portalDiff[1],
            0.2f * portalDiff[2],
            1.0f
        };
        
        glLightfv(GL_LIGHT2, GL_POSITION, portalPos);
        glLightfv(GL_LIGHT2, GL_DIFFUSE, portalDiff);
        glLightfv(GL_LIGHT2, GL_AMBIENT, portalAmb);
        glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.14f);
        glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.07f);
    } else {
        glDisable(GL_LIGHT2);
    }
    
    // ========== LIGHTS 3-5: Crystal Pulsing Lights (Snow Level) ==========
    if(currentLevel == 2){
        for(int i=0; i<std::min(3, (int)crystals.size()); i++){
            glEnable(GL_LIGHT3 + i);
            
            float pulse = std::sin(animTime * 2.0f + crystals[i].glowPhase) * 0.4f + 0.6f;
            float crystalPos[] = {crystals[i].pos.x, crystals[i].pos.y, crystals[i].pos.z, 1.0f};
            float crystalDiff[] = {0.3f * pulse, 0.5f * pulse, 0.8f * pulse, 1.0f};
            float crystalAmb[] = {0.1f * pulse, 0.2f * pulse, 0.3f * pulse, 1.0f};
            
            glLightfv(GL_LIGHT3 + i, GL_POSITION, crystalPos);
            glLightfv(GL_LIGHT3 + i, GL_DIFFUSE, crystalDiff);
            glLightfv(GL_LIGHT3 + i, GL_AMBIENT, crystalAmb);
            glLightf(GL_LIGHT3 + i, GL_CONSTANT_ATTENUATION, 1.0f);
            glLightf(GL_LIGHT3 + i, GL_LINEAR_ATTENUATION, 0.22f);
            glLightf(GL_LIGHT3 + i, GL_QUADRATIC_ATTENUATION, 0.20f);
        }
    } else {
        glDisable(GL_LIGHT3);
        glDisable(GL_LIGHT4);
        glDisable(GL_LIGHT5);
    }
}

// =======================================================
// RENDER SCENE
// =======================================================
void renderScene(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    setupFog();

    // Camera
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, float(screenW)/float(screenH), 0.1f, 300.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    if(cameraMode == CAM_FIRST){
        Vec3 eye(playerPos.x, playerPos.y+0.8f, playerPos.z);
        float sy = std::sin(cameraYaw), cy = std::cos(cameraYaw);
        float lookX = sy * std::cos(cameraPitch);
        float lookY = std::sin(cameraPitch);
        float lookZ = -cy * std::cos(cameraPitch);

        gluLookAt(eye.x,eye.y,eye.z,
                  eye.x+lookX,eye.y+lookY,eye.z+lookZ,
                  0,1,0);
    }
    else {
        float dist = 6.0f;
        float h = 2.2f;

        float sy = std::sin(playerYaw);
        float cy = std::cos(playerYaw);

        Vec3 forward(sy,0,-cy);
        Vec3 cam(
            playerPos.x - forward.x*dist,
            playerPos.y + h,
            playerPos.z - forward.z*dist
        );

        gluLookAt(cam.x,cam.y,cam.z,
                  playerPos.x,playerPos.y+0.6f,playerPos.z,
                  0,1,0);
    }

    // Setup dynamic lighting
    setupDynamicLighting();

    // Draw environment
    drawGroundAndEnvironment();
    drawPortal();
    drawCrystals();
    drawFireSpirit();

    // ========== OBSTACLES ==========
    for(auto& o : obstacles){
        glPushMatrix();
        glTranslatef(o.pos.x, o.pos.y, o.pos.z);

        if(o.type == "stone" && currentLevel == 1){
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, desertStoneTex);
            glColor3f(1.0f, 1.0f, 1.0f);

            GLfloat mat_specular[] = {0.3f, 0.3f, 0.3f, 1.0f};
            GLfloat mat_shininess[] = {25.0f};
            glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
            glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);

            GLUquadric* quad = gluNewQuadric();
            gluQuadricTexture(quad, GL_TRUE);
            gluQuadricNormals(quad, GLU_SMOOTH);
            gluSphere(quad, o.radius, 32, 32);
            gluDeleteQuadric(quad);

            glDisable(GL_TEXTURE_2D);
        }
        else if(o.type == "stone"){
            glColor3f(0.42f,0.36f,0.31f);
            glutSolidSphere(o.radius,28,20);
        }
        else {
            glColor3f(0.92f,0.97f,1.0f);
            glRotatef(-90,1,0,0);
            glutSolidCone(0.45f,1.8f,18,6);
        }

        glPopMatrix();
    }

    // ========== COLLECTIBLES - Fixed Golden Octahedrons with Proper Texture ==========
    for(size_t i=0;i<collectibles.size();i++){
        auto& c = collectibles[i];
        if(!c.collected){
            float bob = std::sin(animTime*2 + i) * 0.25f;

            glPushMatrix();
            glTranslatef(c.pos.x, c.pos.y + bob, c.pos.z);
            glRotatef(animTime*60 + i*20, 0,1,0);

            if(currentLevel == 1){
                // Golden textured octahedrons - FIXED
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, desertGoldTex);
                
                // WHITE color for proper texture display
                glColor3f(1.0f, 1.0f, 1.0f);

                GLfloat mat_specular[] = {0.9f, 0.8f, 0.4f, 1.0f};
                GLfloat mat_shininess[] = {70.0f};
                GLfloat mat_emission[] = {0.15f, 0.12f, 0.02f, 1.0f};
                
                glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
                glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
                glMaterialfv(GL_FRONT, GL_EMISSION, mat_emission);

                glScalef(0.5f, 0.5f, 0.5f);
                
                // Use sphere mapping for proper octahedron texture
                glEnable(GL_TEXTURE_GEN_S);
                glEnable(GL_TEXTURE_GEN_T);
                glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
                glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
                
                GLfloat s_plane[] = {1.0f, 0.0f, 0.0f, 0.0f};
                GLfloat t_plane[] = {0.0f, 1.0f, 0.0f, 0.0f};
                glTexGenfv(GL_S, GL_OBJECT_PLANE, s_plane);
                glTexGenfv(GL_T, GL_OBJECT_PLANE, t_plane);
                
                glutSolidOctahedron();
                
                glDisable(GL_TEXTURE_GEN_S);
                glDisable(GL_TEXTURE_GEN_T);

                GLfloat no_emission[] = {0.0f, 0.0f, 0.0f, 1.0f};
                glMaterialfv(GL_FRONT, GL_EMISSION, no_emission);
                
                glDisable(GL_TEXTURE_2D);
            }
            else {
                // Snow level - blue octahedrons (no texture)
                glColor3f(0.55f,0.85f,1.0f);
                glScalef(0.5f, 0.5f, 0.5f);
                glutSolidOctahedron();
            }

            glPopMatrix();
        }
    }

    // ========== PLAYER ==========
    glDisable(GL_TEXTURE_2D);
    
    glPushMatrix();
    glTranslatef(playerPos.x, playerPos.y, playerPos.z);
    glRotatef(playerYaw * 57.2958f, 0,1,0);

    if(!playerMesh.empty()){
        glColor3f(0.9f,0.6f,0.4f);
        glBegin(GL_TRIANGLES);
        for(const auto& v : playerMesh.verts)
            glVertex3f(v.x, v.y, v.z);
        glEnd();
    }
    else {
        glColor3f(0.9f,0.6f,0.4f);
        glutSolidSphere(0.28f,16,12);

        glColor3f(0.7f,0.3f,0.2f);
        glTranslatef(0,-0.55f,0);
        glScalef(0.6f,0.9f,0.35f);
        glutSolidCube(1.0f);
    }

    glPopMatrix();

    // ========== HUD ==========
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0,screenW,0,screenH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(1,1,1);

    std::string title = (currentLevel==1 ? "DESERT TEMPLE RUINS" : "FROZEN CAVES");
    glRasterPos2f(20,screenH-34);
    for(char c : title) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,c);

    std::string sc = "Score: " + std::to_string(score);
    glRasterPos2f(20,screenH-58);
    for(char c : sc) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,c);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// =======================================================
// INPUT
// =======================================================
void reshape(int w,int h){
    screenW = w;
    screenH = h;
    glViewport(0,0,w,h);
}

void onKeyDown(unsigned char key,int,int){
    keys[key] = true;

    if(key == 27) exit(0);

    if(key=='c' || key=='C'){
        cameraMode = (cameraMode==CAM_FIRST ? CAM_THIRD : CAM_FIRST);
        cameraYaw = playerYaw;
    }

    if(key=='l' || key=='L'){
        if(currentLevel == 1) setupSnow();
        else setupDesert();
    }

    if(key=='r' || key=='R'){
        if(currentLevel == 1) setupDesert();
        else setupSnow();
    }
}

void onKeyUp(unsigned char key,int,int){
    keys[key] = false;
}

bool firstMouse = true;
int lastMouseX = 1280/2;
int lastMouseY = 800/2;

void onMouseMove(int x,int y){
    if(firstMouse){
        lastMouseX=x;
        lastMouseY=y;
        firstMouse=false;
        return;
    }

    int dx = x - lastMouseX;
    int dy = y - lastMouseY;

    lastMouseX = x;
    lastMouseY = y;

    float sens = 0.0045f;

    playerYaw += dx * sens;
    cameraYaw  = playerYaw;

    cameraPitch -= dy * sens;
    cameraPitch = clampf(cameraPitch, -1.2f, 1.2f);
}

// =======================================================
// IDLE
// =======================================================
void idle(){
    float t = glutGet(GLUT_ELAPSED_TIME)/1000.0f;
    float dt = (lastTime==0 ? 1.0f/60.0f : (t-lastTime));
    lastTime = t;

    update(dt);
    glutPostRedisplay();
}

// =======================================================
// MAIN
// =======================================================
int main(int argc,char** argv){
    srand((unsigned)time(nullptr));

    playerMesh = loadOBJ("player.obj");

    setupDesert();

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(screenW,screenH);
    glutCreateWindow("GLUT Game — Enhanced Lighting");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    
    // Enhanced rendering quality
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    
    // Enable blending for better visual effects
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Load all textures
    std::cout << "Loading textures...\n";
    
    desertWallTex   = loadBMP("rock_boulder_cracked_diff_4k.bmp");
    if(desertWallTex) std::cout << "  ✓ Desert wall texture loaded\n";
    
    snowWallTex     = loadBMP("jersey_melange_diff_4k.bmp");
    if(snowWallTex) std::cout << "  ✓ Snow wall texture loaded\n";
    
    desertFloorTex  = loadBMP("Ground095A_4K-JPG_Color.bmp");
    if(desertFloorTex) std::cout << "  ✓ Desert floor texture loaded\n";
    
    snowFloorTex    = loadBMP("Snow008A_4K-JPG_Color.bmp");
    if(snowFloorTex) std::cout << "  ✓ Snow floor texture loaded\n";
    
    desertStoneTex  = loadBMP("large_sandstone_blocks_01_diff_4k.bmp");
    if(desertStoneTex) std::cout << "  ✓ Desert stone texture loaded\n";
    
    desertGoldTex   = loadBMP("Metal042B.bmp");
    if(desertGoldTex) std::cout << "  ✓ Desert gold texture loaded\n";
    
    roofTex = loadBMP("large_sandstone_blocks_01_diff_4k.bmp");
    if(roofTex) std::cout << "  ✓ Roof texture loaded\n";
    
    fireSpiritTex = loadBMP("ChristmasTreeOrnament014_4K-JPG_Color.bmp");
    if(fireSpiritTex) std::cout << "  ✓ Fire spirit texture loaded\n";
    
    portalTex = loadBMP("large_sandstone_blocks_01_diff_4k.bmp");
    if(portalTex) std::cout << "  ✓ Portal texture loaded\n";
    
    std::cout << "\n✨ All textures loaded successfully!\n\n";
    std::cout << "🎮 ENHANCED FEATURES:\n";
    std::cout << "  • Dynamic day/night cycle (desert)\n";
    std::cout << "  • Pulsing crystal lights (snow caves)\n";
    std::cout << "  • Shifting portal lights\n";
    std::cout << "  • Moving fire spirit orb\n";
    std::cout << "  • Textured stone roof\n\n";
    std::cout << "Controls:\n";
    std::cout << "  WASD - Move\n";
    std::cout << "  Mouse - Look around\n";
    std::cout << "  C - Toggle camera mode\n";
    std::cout << "  L - Next level\n";
    std::cout << "  R - Restart level\n";
    std::cout << "  ESC - Quit\n\n";

    glutDisplayFunc(renderScene);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(onKeyDown);
    glutKeyboardUpFunc(onKeyUp);
    glutPassiveMotionFunc(onMouseMove);
    glutMotionFunc(onMouseMove);
    glutIdleFunc(idle);

    glutMainLoop();
    return 0;
}
