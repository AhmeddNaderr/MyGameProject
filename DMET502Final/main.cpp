
// main.cpp — Full merged version with:
// Level 1 Walls  → rock_boulder_cracked_diff_4k.bmp
// Level 2 Walls  → jersey_melange_diff_4k.bmp
// Level 1 Floor  → Ground095A_4K-JPG_Color.bmp
// Level 2 Floor  → Snow008A_4K-JPG_Color.bmp

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
// SIMPLE BMP TEXTURE LOADER
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
        std::cout << "ERROR: Not a valid BMP file.\n";
        fclose(file);
        return 0;
    }

    if (header[0] != 'B' || header[1] != 'M') {
        std::cout << "ERROR: Invalid BMP header.\n";
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

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
                 0, GL_BGR, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    delete[] data;
    return texID;
}

// =======================================================
// BASIC MATH
// =======================================================
struct Vec3 { float x,y,z;
    Vec3() : x(0),y(0),z(0) {}
    Vec3(float X,float Y,float Z):x(X),y(Y),z(Z){}
};

inline float frand(float a,float b){
    return a + (b-a) * (float(rand()) / float(RAND_MAX));
}
inline float len(const Vec3& v){
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}
inline float clampf(float v,float a,float b){
    return v < a ? a : (v > b ? b : v);
}

// =======================================================
// GLOBAL VARIABLES
// =======================================================
const float WORLD_HALF  = 36.0f;
const float WALL_HEIGHT = 7.0f;

Vec3 playerPos(0,1.0f,0), lastSafePos(0,1.0f,0);
float playerYaw=0, cameraYaw=0, playerPitch=0, cameraPitch=0;

float lastTime=0, playerRadius=0.6f, playerSpeed=8.0f;
bool keys[512]={false};
int screenW=1280, screenH=800;

enum CameraMode { CAM_THIRD, CAM_FIRST };
CameraMode cameraMode = CAM_THIRD;

int currentLevel = 1;
int score = 0;

// =======================================================
// TEXTURES — ADDED FLOOR TEXTURES
// =======================================================
GLuint desertWallTex = 0;     // Level 1 walls
GLuint snowWallTex   = 0;     // Level 2 walls
GLuint desertFloorTex = 0;    // Level 1 floor
GLuint snowFloorTex   = 0;    // Level 2 floor

// =======================================================
// SIMPLE OBJ LOADER
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
        if(line.size() < 2) continue;
        std::istringstream iss(line);
        std::string tag; iss >> tag;

        if(tag=="v"){
            float x,y,z; iss>>x>>y>>z;
            vlist.push_back({x,y,z});
        }
        else if(tag=="f"){
            std::string w;
            std::vector<std::string> f;
            while(iss>>w) f.push_back(w);

            auto idx=[&](std::string s){
                size_t p=s.find('/');
                int i=(p==std::string::npos? stoi(s):stoi(s.substr(0,p)));
                if(i<0) i = int(vlist.size())+1+i;
                return i-1;
            };

            for(size_t i=1;i+1<f.size();i++){
                mesh.verts.push_back(vlist[idx(f[0])]);
                mesh.verts.push_back(vlist[idx(f[i])]);
                mesh.verts.push_back(vlist[idx(f[i+1])]);
            }
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
// RANDOM POSITION GENERATOR
// =======================================================
Vec3 randomUniformPosition(float minDist){
    while(true){
        float x = frand(-WORLD_HALF+3, WORLD_HALF-3);
        float z = frand(-WORLD_HALF+3, WORLD_HALF-3);

        Vec3 p(x,0,z);
        bool ok=true;

        for(auto &c:collectibles)
            if(len(Vec3{p.x-c.pos.x,0,p.z-c.pos.z})<minDist) ok=false;
        for(auto &o:obstacles)
            if(len(Vec3{p.x-o.pos.x,0,p.z-o.pos.z})<minDist) ok=false;

        if(ok) return p;
    }
}
// =======================================================
// LEVEL SETUP
// =======================================================

void clearLevel(){
    collectibles.clear();
    obstacles.clear();
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
    portal.radius = 2.8f;
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

    portal.pos = Vec3(0,0,-(WORLD_HALF - 4.0f));
    portal.radius = 2.8f;
}

// =======================================================
// DRAW ENVIRONMENT (Walls + Floor Textured by Level)
// =======================================================

void drawGroundAndEnvironment(){
    float half = WORLD_HALF;
    float h    = WALL_HEIGHT;

    // Background sky color
    if(currentLevel == 1)
        glClearColor(0.96f, 0.90f, 0.75f, 1.0f);
    else
        glClearColor(0.68f, 0.82f, 0.98f, 1.0f);

    // ======================================================
    // FLOOR TEXTURES — NEW FEATURE
    // ======================================================

    if(currentLevel == 1){
        glBindTexture(GL_TEXTURE_2D, desertFloorTex);
        glColor3f(1,1,1);
    }
    else if(currentLevel == 2){
        glBindTexture(GL_TEXTURE_2D, snowFloorTex);
        glColor3f(1,1,1);
    }
    else{
        glBindTexture(GL_TEXTURE_2D, 0);
        glColor3f(1,1,1);
    }

    float floorRepeat = 10.0f;

    glBegin(GL_QUADS);
        glNormal3f(0,1,0);
        glTexCoord2f(0,0);                    glVertex3f(-half,0,-half);
        glTexCoord2f(floorRepeat,0);          glVertex3f( half,0,-half);
        glTexCoord2f(floorRepeat,floorRepeat); glVertex3f( half,0, half);
        glTexCoord2f(0,floorRepeat);          glVertex3f(-half,0, half);
    glEnd();

    // ======================================================
    // WALL TEXTURES
    // ======================================================
    if(currentLevel == 1){
        glBindTexture(GL_TEXTURE_2D, desertWallTex);
        glColor3f(1,1,1);
    }
    else if(currentLevel == 2){
        glBindTexture(GL_TEXTURE_2D, snowWallTex);
        glColor3f(1,1,1);
    }
    else {
        glBindTexture(GL_TEXTURE_2D, 0);
        glColor3f(1,1,1);
    }

    float repeat = 4.0f;

    // +Z wall
    glBegin(GL_QUADS);
        glNormal3f(0,0,-1);
        glTexCoord2f(0,0);           glVertex3f(-half,0, half);
        glTexCoord2f(repeat,0);      glVertex3f( half,0, half);
        glTexCoord2f(repeat,repeat); glVertex3f( half,h, half);
        glTexCoord2f(0,repeat);      glVertex3f(-half,h, half);
    glEnd();

    // -Z wall
    glBegin(GL_QUADS);
        glNormal3f(0,0,1);
        glTexCoord2f(0,0);           glVertex3f(-half,0,-half);
        glTexCoord2f(0,repeat);      glVertex3f(-half,h,-half);
        glTexCoord2f(repeat,repeat); glVertex3f( half,h,-half);
        glTexCoord2f(repeat,0);      glVertex3f( half,0,-half);
    glEnd();

    // -X wall
    glBegin(GL_QUADS);
        glNormal3f(1,0,0);
        glTexCoord2f(0,0);           glVertex3f(-half,0,-half);
        glTexCoord2f(repeat,0);      glVertex3f(-half,0, half);
        glTexCoord2f(repeat,repeat); glVertex3f(-half,h, half);
        glTexCoord2f(0,repeat);      glVertex3f(-half,h,-half);
    glEnd();

    // +X wall
    glBegin(GL_QUADS);
        glNormal3f(-1,0,0);
        glTexCoord2f(0,0);           glVertex3f(half,0,-half);
        glTexCoord2f(repeat,0);      glVertex3f(half,0, half);
        glTexCoord2f(repeat,repeat); glVertex3f(half,h, half);
        glTexCoord2f(0,repeat);      glVertex3f(half,h,-half);
    glEnd();

    // ROOF (no texture)
    glBindTexture(GL_TEXTURE_2D, 0);
    if(currentLevel == 1) glColor3f(0.75f,0.65f,0.45f);
    else                  glColor3f(0.88f,0.93f,0.98f);

    glBegin(GL_QUADS);
        glNormal3f(0,-1,0);
        glVertex3f(-half,h,-half);
        glVertex3f( half,h,-half);
        glVertex3f( half,h, half);
        glVertex3f(-half,h, half);
    glEnd();
}

// =======================================================
// PLAYER MODEL
// =======================================================

void drawPlayerModel(){
    if(!playerMesh.empty()){
        glPushMatrix();
        glScalef(0.5f,0.5f,0.5f);
        glBegin(GL_TRIANGLES);
        for(const auto& v : playerMesh.verts)
            glVertex3f(v.x, v.y, v.z);
        glEnd();
        glPopMatrix();
    } else {
        glPushMatrix();
        glColor3f(0.9f,0.6f,0.4f);
        glTranslatef(0,0.65f,0);
        glutSolidSphere(0.28f,16,12);
        glPopMatrix();

        glPushMatrix();
        glColor3f(0.7f,0.3f,0.2f);
        glTranslatef(0,0.1f,0);
        glScalef(0.6f,0.9f,0.35f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }
}

// =======================================================
// PORTAL
// =======================================================

void drawPortal(){
    glPushMatrix();
    glTranslatef(portal.pos.x, portal.pos.y + 1.2f, portal.pos.z);

    float t = glutGet(GLUT_ELAPSED_TIME) * 0.05f;

    if(currentLevel == 1){
        glPushMatrix();
        glColor3f(1.0f,0.85f,0.35f);
        glRotatef(90,0,1,0);
        glScalef(1.4f,2.3f,0.4f);
        glutSolidTorus(0.22f,1.32f,28,28);
        glPopMatrix();

        glColor3f(1.0f,0.55f,0.15f);
        glRotatef(t,0,1,0);
        glutSolidTorus(0.11f,0.99f,24,24);
    }
    else {
        glPushMatrix();
        glColor3f(0.80f,0.90f,1.0f);
        glRotatef(90,0,1,0);
        glScalef(1.6f,2.5f,0.4f);
        glutSolidTorus(0.198f,1.21f,28,28);
        glPopMatrix();

        glColor3f(0.55f,0.80f,1.0f);
        glRotatef(-t,0,1,0);
        glutSolidTorus(0.132f,0.935f,24,24);
    }

    glPopMatrix();
}

// =======================================================
// OBSTACLE PHYSICS
// =======================================================

void integrateObstacles(float dt){
    for(auto& o : obstacles){
        if(o.type == "icicle" && !o.grounded){
            const float slowG = -4.2f;
            o.vel.y += slowG * dt;
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

    if(pushedBack){
        for(auto& o : obstacles){
            if(distXZ(playerPos, o.pos) < o.radius + playerRadius - 0.01f){
                playerPos = lastSafePos;
                break;
            }
        }
    }
}
// =======================================================
// UPDATE LOOP
// =======================================================

void update(float dt){
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

    // Collectibles pickup
    for(auto& c : collectibles){
        if(!c.collected && distXZ(playerPos, c.pos) < c.radius + playerRadius + 0.15f){
            c.collected = true;
            score += 10;
        }
    }

    bool allCollected = true;
    for(auto& c : collectibles) if(!c.collected) allCollected = false;

    // Portal to switch levels
    if(allCollected && distXZ(playerPos, portal.pos) < portal.radius + 0.8f){
        if(currentLevel == 1) setupSnow();
        else setupDesert();
    }

    // Arena boundaries
    float bound = WORLD_HALF - playerRadius - 0.1f;
    playerPos.x = clampf(playerPos.x, -bound, bound);
    playerPos.z = clampf(playerPos.z, -bound, bound);
}

// =======================================================
// RENDER
// =======================================================

void renderScene(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0f, float(screenW)/float(screenH), 0.1f, 300.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Camera
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
        Vec3 cam(playerPos.x - forward.x*dist,
                 playerPos.y + h,
                 playerPos.z - forward.z*dist);

        gluLookAt(cam.x,cam.y,cam.z,
                  playerPos.x,playerPos.y+0.6f,playerPos.z,
                  0,1,0);
    }

    // Lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    if(currentLevel == 1){
        float sun[]  = {18,45,12,1};
        float diff[] = {1.0,0.92,0.70,1};
        float spec[] = {1.0,0.95,0.75,1};
        float amb[]  = {0.35,0.28,0.20,1};
        glLightfv(GL_LIGHT0, GL_POSITION, sun);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_SPECULAR,spec);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    }
    else {
        float sun[]  = {12,50,18,1};
        float diff[] = {0.75,0.85,1.0,1};
        float spec[] = {0.85,0.90,1.0,1};
        float amb[]  = {0.32,0.36,0.45,1};
        glLightfv(GL_LIGHT0, GL_POSITION, sun);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diff);
        glLightfv(GL_LIGHT0, GL_SPECULAR,spec);
        glLightfv(GL_LIGHT0, GL_AMBIENT, amb);
    }

    // Draw world
    drawGroundAndEnvironment();
    drawPortal();

    // Obstacles
    for(auto& o : obstacles){
        glPushMatrix();
        glTranslatef(o.pos.x, o.pos.y, o.pos.z);
        if(o.type == "stone"){
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

    // Collectibles
    float t = glutGet(GLUT_ELAPSED_TIME)/1000.0f;
    for(size_t i=0;i<collectibles.size();i++){
        auto& c = collectibles[i];
        if(!c.collected){
            float bob = std::sin(t*2 + i)*0.25f;
            glPushMatrix();
            glTranslatef(c.pos.x, c.pos.y + bob, c.pos.z);
            glRotatef(t*60 + i*20, 0,1,0);
            if(currentLevel == 1) glColor3f(1.0,0.85,0.10);
            else                  glColor3f(0.55,0.85,1.0);
            glutSolidOctahedron();
            glPopMatrix();
        }
    }

    // Player (only in third-person)
    if(cameraMode == CAM_THIRD){
        glPushMatrix();
        glTranslatef(playerPos.x, playerPos.y, playerPos.z);
        glRotatef(playerYaw * 57.2958f, 0,1,0);
        drawPlayerModel();
        glPopMatrix();
    }

    // HUD
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

    std::string title = (currentLevel==1 ? "DESERT TEMPLE RUINS" : "FROZEN CAVES (SNOW)");
    glRasterPos2f(20,screenH-34);
    for(char c : title) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,c);

    std::string sc = "Score: " + std::to_string(score);
    glRasterPos2f(20,screenH-58);
    for(char c : sc) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,c);

    int collectedCount = 0;
    for(auto& c : collectibles) if(c.collected) collectedCount++;
    std::string col = "Collected: " + std::to_string(collectedCount) + "/" + std::to_string(collectibles.size());
    glRasterPos2f(200,screenH-58);
    for(char c : col) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,c);

    std::string help = "WASD move | Mouse look | C camera | L switch level | R reset";
    glRasterPos2f(20,18);
    for(char c : help) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12,c);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glEnable(GL_DEPTH_TEST);

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
// IDLE LOOP
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
    if(!playerMesh.empty())
        std::cout << "Loaded player.obj\n";
    else
        std::cout << "Fallback player model used.\n";

    setupDesert();

    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(screenW,screenH);
    glutCreateWindow("GLUT Game — Textured Desert & Snow");

    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_TEXTURE_2D);

    // LOAD ALL TEXTURES
    desertWallTex   = loadBMP("rock_boulder_cracked_diff_4k.bmp");
    snowWallTex     = loadBMP("jersey_melange_diff_4k.bmp");
    desertFloorTex  = loadBMP("Ground095A_4K-JPG_Color.bmp");
    snowFloorTex    = loadBMP("Snow008A_4K-JPG_Color.bmp");

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
