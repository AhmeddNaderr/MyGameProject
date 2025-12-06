// DMET 502 - Computer Graphics Final Project
// Generic 3D Game Engine Skeleton (macOS + OpenGL + GLUT)
//
// - Two levels (different layouts, colors, lights, textures)
// - First-person & third-person cameras (switch with '1' and '3')
// - Keyboard + mouse navigation
// - Player, obstacles, collectibles
// - Collision detection
// - Textured environment (ground, walls, objects)
// - Lighting with color changes + animated light source
// - Score calculation displayed on screen
// - Simple interaction animations (spinning coins, player reaction)
//
// NOTE:
//  * This is a base engine. You MUST customize:
//      - Theme, models (.3ds), textures, sounds
//      - Level designs and goals
//  * Texture loader here assumes 24-bit uncompressed BMP for simplicity.
//    You can replace it with stb_image or SOIL if you prefer PNG/JPG.
//

#define GL_SILENCE_DEPRECATION   // silence macOS OpenGL deprecation warnings
#include "ObjModel.h"
ObjModel playerModel;

#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

// -----------------------------------------
// Math helpers
// -----------------------------------------

struct Vec3 {
    float x, y, z;
    Vec3(float X = 0, float Y = 0, float Z = 0) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3 &o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3 &o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
};

struct AABB {
    Vec3 min, max;
};

// Simple AABB collision
bool intersects(const AABB &a, const AABB &b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

// -----------------------------------------
// Global state
// -----------------------------------------

int windowWidth  = 1280;
int windowHeight = 720;

float lastMouseX = windowWidth / 2.0f;
float lastMouseY = windowHeight / 2.0f;
bool  firstMouse = true;

bool  keyStates[256]; // for smooth WASD

// Game state
enum LevelID { LEVEL_1 = 0, LEVEL_2 = 1 };
LevelID currentLevel = LEVEL_1;

int   score       = 0;
bool  gameOver    = false;
bool  firstPerson = true; // camera mode

// Timing
float deltaTime = 0.016f;
float lastFrame = 0.0f;

// -----------------------------------------
// Texture helper (simple BMP loader)
// -----------------------------------------

GLuint loadBMPTexture(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Could not open texture file %s\n", filename);
        return 0;
    }

    unsigned char header[54];
    fread(header, 1, 54, file);
    if (header[0] != 'B' || header[1] != 'M') {
        printf("Not a valid BMP file: %s\n", filename);
        fclose(file);
        return 0;
    }

    unsigned int dataPos   = *(int*)&(header[0x0A]);
    unsigned int imageSize = *(int*)&(header[0x22]);
    unsigned int width     = *(int*)&(header[0x12]);
    unsigned int height    = *(int*)&(header[0x16]);

    if (imageSize == 0) imageSize = width * height * 3;
    if (dataPos == 0)   dataPos = 54;

    unsigned char *data = new unsigned char[imageSize];
    fseek(file, dataPos, SEEK_SET);
    fread(data, 1, imageSize, file);
    fclose(file);

    // OpenGL texture setup
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // BMP is BGR; tell OpenGL that
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 width, height, 0, GL_BGR,
                 GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    delete[] data;
    return textureID;
}

// -----------------------------------------
// Camera
// -----------------------------------------

struct Camera {
    Vec3 pos;
    float yaw;   // rotation around Y
    float pitch; // rotation around X

    Camera() {
        pos   = Vec3(0.0f, 2.0f, 5.0f);
        yaw   = -90.0f;
        pitch = 0.0f;
    }

    Vec3 front() const {
        float radYaw = yaw * (float)M_PI / 180.0f;
        float radPit = pitch * (float)M_PI / 180.0f;
        Vec3 dir;
        dir.x = std::cos(radYaw) * std::cos(radPit);
        dir.y = std::sin(radPit);
        dir.z = std::sin(radYaw) * std::cos(radPit);
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        return Vec3(dir.x / len, dir.y / len, dir.z / len);
    }

    Vec3 right() const {
        Vec3 f = front();
        Vec3 r(f.z, 0.0f, -f.x);
        float len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
        return Vec3(r.x / len, r.y / len, r.z / len);
    }

    Vec3 up() const {
        Vec3 f = front();
        Vec3 r = right();
        Vec3 u(r.y * f.z - r.z * f.y,
               r.z * f.x - r.x * f.z,
               r.x * f.y - r.y * f.x);
        float len = std::sqrt(u.x * u.x + u.y * u.y + u.z * u.z);
        return Vec3(u.x / len, u.y / len, u.z / len);
    }

    void apply() const {
        Vec3 f = front();
        Vec3 target = pos + f;
        Vec3 u = up();
        gluLookAt(pos.x, pos.y, pos.z,
                  target.x, target.y, target.z,
                  u.x, u.y, u.z);
    }
};

Camera camera;

// -----------------------------------------
// Player
// -----------------------------------------

struct Player {
    Vec3 pos;
    float radius;
    float speed;
    float heading; // for drawing the player facing movement direction

    Player() {
        pos     = Vec3(0.0f, 0.5f, 0.0f);
        radius  = 0.5f;
        speed   = 5.0f;
        heading = 0.0f;
    }

    AABB bounds() const {
        return { Vec3(pos.x - radius, 0.0f, pos.z - radius),
                 Vec3(pos.x + radius, 1.0f, pos.z + radius) };
    }

    void draw() {
        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(heading, 0, 1, 0);
        glTranslatef(0.0f, -1.0f, 0.0f);   // lower the model

        glScalef(0.01f, 0.01f, 0.01f);   // <--- VERY IMPORTANT
        playerModel.draw();
        glPopMatrix();

    }



};

Player player;

// -----------------------------------------
// Obstacles & collectibles
// -----------------------------------------

struct Obstacle {
    Vec3 pos;
    Vec3 size;
    GLuint texture;
    bool harmful; // if true, collision decreases score

    AABB bounds() const {
        return { Vec3(pos.x - size.x * 0.5f, 0.0f, pos.z - size.z * 0.5f),
                 Vec3(pos.x + size.x * 0.5f, size.y, pos.z + size.z * 0.5f) };
    }

    void draw() const {
        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glScalef(size.x, size.y, size.z);
        glutSolidCube(1.0);
        glPopMatrix();
        if (texture) glDisable(GL_TEXTURE_2D);
    }
};

struct Collectible {
    Vec3 pos;
    float radius;
    bool  collected;
    GLuint texture;
    float rotation; // animation

    Collectible(Vec3 p = Vec3()) {
        pos = p;
        radius = 0.4f;
        collected = false;
        texture = 0;
        rotation = 0.0f;
    }

    AABB bounds() const {
        return { Vec3(pos.x - radius, pos.y - radius, pos.z - radius),
                 Vec3(pos.x + radius, pos.y + radius, pos.z + radius) };
    }

    void update(float dt) {
        rotation += 180.0f * dt; // spin animation
        if (rotation > 360.0f) rotation -= 360.0f;
    }

    void draw() const {
        if (collected) return;
        glPushMatrix();
        glTranslatef(pos.x, pos.y, pos.z);
        glRotatef(rotation, 0.0f, 1.0f, 0.0f);
        if (texture) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);
        }
        glColor3f(1.0f, 1.0f, 0.0f);
        glutSolidTorus(0.1, 0.3, 16, 32); // placeholder "coin"
        if (texture) glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }
};

std::vector<Obstacle>    obstacles;
std::vector<Collectible> collectibles;

// -----------------------------------------
// Lighting
// -----------------------------------------

float lightAngle = 0.0f;
bool  dynamicColor = true;

void setupLights() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    // Light 0: main warm light
    GLfloat ambient0[]  = {0.2f, 0.2f, 0.2f, 1.0f};
    GLfloat diffuse0[]  = {0.9f, 0.7f, 0.6f, 1.0f};
    GLfloat specular0[] = {1.0f, 0.9f, 0.8f, 1.0f};
    glLightfv(GL_LIGHT0, GL_AMBIENT,  ambient0);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  diffuse0);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular0);

    // Light 1: colored moving light
    GLfloat ambient1[]  = {0.0f, 0.0f, 0.0f, 1.0f};
    GLfloat diffuse1[]  = {0.0f, 0.6f, 1.0f, 1.0f};
    GLfloat specular1[] = {0.0f, 0.8f, 1.0f, 1.0f};
    glLightfv(GL_LIGHT1, GL_AMBIENT,  ambient1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  diffuse1);
    glLightfv(GL_LIGHT1, GL_SPECULAR, specular1);
}

void updateLights(float dt) {
    lightAngle += 20.0f * dt;
    if (lightAngle > 360.0f) lightAngle -= 360.0f;
    float radius = 10.0f;

    GLfloat position1[] = {
        (GLfloat)(radius * std::cos(lightAngle * (float)M_PI / 180.0f)),
        5.0f,
        (GLfloat)(radius * std::sin(lightAngle * (float)M_PI / 180.0f)),
        1.0f
    };
    glLightfv(GL_LIGHT1, GL_POSITION, position1);

    if (dynamicColor) {
        float t = (std::sin(lightAngle * (float)M_PI / 180.0f) + 1.0f) * 0.5f;
        GLfloat diffuse1[] = {t, 0.6f * (1.0f - t), 1.0f, 1.0f};
        glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuse1);
    }

    GLfloat position0[] = {0.0f, 10.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, position0);
}

// -----------------------------------------
// Levels
// -----------------------------------------

GLuint texGround1 = 0, texGround2 = 0, texWall1 = 0, texWall2 = 0;

void buildLevel(LevelID level) {
    obstacles.clear();
    collectibles.clear();

    if (level == LEVEL_1) {
        // Level 1
        for (int i = -5; i <= 5; ++i) {
            Obstacle wall;
            wall.pos = Vec3(i * 2.0f, 1.0f, -10.0f);
            wall.size = Vec3(1.5f, 2.0f, 1.0f);
            wall.texture = texWall1;
            wall.harmful = true;
            obstacles.push_back(wall);
        }

        for (int i = 0; i < 5; ++i) {
            Obstacle b;
            b.pos = Vec3((i - 2) * 3.0f, 1.0f, (i - 1) * 3.0f);
            b.size = Vec3(2.0f, 2.0f, 2.0f);
            b.texture = texWall1;
            b.harmful = true;
            obstacles.push_back(b);
        }

        collectibles.emplace_back(Vec3(-4.0f, 1.0f, -5.0f));
        collectibles.emplace_back(Vec3(0.0f,  1.0f, -8.0f));
        collectibles.emplace_back(Vec3(4.0f,  1.0f, -2.0f));

    } else {
        // Level 2
        for (int i = -5; i <= 5; ++i) {
            Obstacle wall;
            wall.pos = Vec3(-10.0f, 1.0f, i * 2.0f);
            wall.size = Vec3(1.5f, 2.0f, 1.5f);
            wall.texture = texWall2;
            wall.harmful = true;
            obstacles.push_back(wall);
        }

        for (int i = 0; i < 6; ++i) {
            Obstacle pillar;
            pillar.pos  = Vec3((i - 3) * 3.0f, 1.5f,
                               (i % 2 == 0) ? -4.0f : 4.0f);
            pillar.size = Vec3(1.5f, 3.0f, 1.5f);
            pillar.texture = texWall2;
            pillar.harmful = true;
            obstacles.push_back(pillar);
        }

        collectibles.emplace_back(Vec3(-6.0f, 1.0f,  0.0f));
        collectibles.emplace_back(Vec3( 0.0f, 1.0f,  6.0f));
        collectibles.emplace_back(Vec3( 6.0f, 1.0f, -6.0f));
        collectibles.emplace_back(Vec3( 3.0f, 1.0f,  3.0f));
    }
}

// -----------------------------------------
// Rendering helpers
// -----------------------------------------

void drawText(float x, float y, const char *str,
              void *font = GLUT_BITMAP_HELVETICA_18) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    glRasterPos2f(x, y);

    for (const char *c = str; *c != '\0'; ++c) {
        glutBitmapCharacter(font, *c);
    }
    glEnable(GL_LIGHTING);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void drawGround(LevelID level) {
    GLuint tex = (level == LEVEL_1) ? texGround1 : texGround2;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(1.0f, 1.0f, 1.0f);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f,  0.0f);  glVertex3f(-50.0f, 0.0f, -50.0f);
    glTexCoord2f(10.0f, 0.0f);  glVertex3f( 50.0f, 0.0f, -50.0f);
    glTexCoord2f(10.0f,10.0f);  glVertex3f( 50.0f, 0.0f,  50.0f);
    glTexCoord2f(0.0f, 10.0f);  glVertex3f(-50.0f, 0.0f,  50.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
}

// -----------------------------------------
// Interaction logic
// -----------------------------------------

void handleCollisions() {
    AABB playerBox = player.bounds();

    for (auto &o : obstacles) {
        if (intersects(playerBox, o.bounds())) {
            if (o.harmful) {
                score -= 10;
                Vec3 dir = camera.front();
                player.pos = player.pos - dir * 0.5f;
            }
        }
    }

    for (auto &c : collectibles) {
        if (!c.collected && intersects(playerBox, c.bounds())) {
            c.collected = true;
            score += 50;
            printf("Collected! Score = %d\n", score);
            // system("afplay coin.wav &"); // optional sound
        }
    }

    bool allCollected = true;
    for (const auto &c : collectibles) {
        if (!c.collected) {
            allCollected = false;
            break;
        }
    }
    if (allCollected) {
        gameOver = true;
    }
}

// -----------------------------------------
// Game update
// -----------------------------------------
void updateGame(float dt) {
    // -----------------------------
    // Movement relative to camera
    // -----------------------------
    Vec3 move(0.0f, 0.0f, 0.0f);
    Vec3 front = camera.front();
    Vec3 right = camera.right();

    // ignore vertical tilt for movement
    front.y = 0.0f;
    right.y = 0.0f;

    if (keyStates['w'] || keyStates['W']) move = move + front;
    if (keyStates['s'] || keyStates['S']) move = move - front;
    if (keyStates['a'] || keyStates['A']) move = move - right;
    if (keyStates['d'] || keyStates['D']) move = move + right;

    if (move.x != 0.0f || move.z != 0.0f) {
        float len = std::sqrt(move.x * move.x + move.z * move.z);
        move.x /= len;
        move.z /= len;
        player.pos = player.pos + move * (player.speed * dt);

        // update player rotation to face movement direction
        player.heading = std::atan2(move.z, move.x) * 180.0f / (float)M_PI - 90.0f;
    }

    // -----------------------------
    // Camera position
    // -----------------------------
    Vec3 playerCenter(player.pos.x,
                      player.pos.y + 1.2f, // head/center height
                      player.pos.z);

    if (firstPerson) {
        // First-person: camera at player head
        camera.pos = playerCenter;
    } else {
        // Third-person: orbit camera behind player

        float distance = 5.0f;   // how far behind
        float ry = camera.yaw   * (float)M_PI / 180.0f;
        float rp = camera.pitch * (float)M_PI / 180.0f;

        // Clamp pitch for 3rd person so camera never goes crazy
        const float maxPitch = 45.0f * (float)M_PI / 180.0f;
        const float minPitch = -20.0f * (float)M_PI / 180.0f;
        if (rp > maxPitch) rp = maxPitch;
        if (rp < minPitch) rp = minPitch;

        // Spherical coordinates around player
        float offsetX = distance * std::cos(rp) * std::cos(ry);
        float offsetY = distance * std::sin(rp);
        float offsetZ = distance * std::cos(rp) * std::sin(ry);

        // camera is behind the player (center - offset)
        camera.pos = Vec3(
            playerCenter.x - offsetX,
            playerCenter.y - offsetY,
            playerCenter.z - offsetZ
        );
    }

    // -----------------------------
    // Update collectibles animation
    // -----------------------------
    for (auto &c : collectibles) {
        c.update(dt);
    }

    // -----------------------------
    // Collision handling
    // -----------------------------
    handleCollisions();
}

// -----------------------------------------
// GLUT callbacks
// -----------------------------------------
void displayCallback() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    if (firstPerson) {
        // FPS: use yaw + pitch
        camera.apply();
    } else {
        // Third-person: always look at player from camera.pos
        Vec3 target(player.pos.x,
                    player.pos.y + 1.2f,
                    player.pos.z);

        gluLookAt(
            camera.pos.x, camera.pos.y, camera.pos.z,
            target.x,     target.y,     target.z,
            0.0f, 1.0f, 0.0f
        );
    }

    updateLights(deltaTime);
    // (rest of your drawing code exactly the same)
    drawGround(currentLevel);

    for (const auto &o : obstacles)    o.draw();
    for (const auto &c : collectibles) c.draw();

    player.draw();

    char hud[128];
    std::snprintf(hud, sizeof(hud),
                  "Score: %d   Level: %d   Camera: %s",
                  score,
                  (currentLevel == LEVEL_1 ? 1 : 2),
                  (firstPerson ? "First" : "Third"));
    drawText(10.0f, windowHeight - 25.0f, hud);

    if (gameOver) {
        drawText(windowWidth / 2.0f - 80.0f,
                 windowHeight / 2.0f,
                 "LEVEL COMPLETE!");
    }

    glutSwapBuffers();
}

void reshapeCallback(int w, int h) {
    windowWidth  = w;
    windowHeight = (h == 0) ? 1 : h;

    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)windowWidth / (double)windowHeight,
                   0.1, 200.0);
    glMatrixMode(GL_MODELVIEW);
}

void keyboardDownCallback(unsigned char key, int, int) {
    keyStates[key] = true;

    if (key == 27) { // ESC
        std::exit(0);
    }

    if (key == '1') {
        firstPerson = true;
    } else if (key == '3') {
        firstPerson = false;
    } else if (key == 'l' || key == 'L') {
        currentLevel = (currentLevel == LEVEL_1) ? LEVEL_2 : LEVEL_1;
        buildLevel(currentLevel);
        gameOver = false;
    } else if (key == 'r' || key == 'R') {
        score = 0;
        player = Player();
        buildLevel(currentLevel);
        gameOver = false;
    }
}

void keyboardUpCallback(unsigned char key, int, int) {
    keyStates[key] = false;
}

void mouseMotionCallback(int x, int y) {
    if (firstMouse) {
        lastMouseX = (float)x;
        lastMouseY = (float)y;
        firstMouse = false;
    }

    float xoffset = (float)x - lastMouseX;
    float yoffset = lastMouseY - (float)y;
    lastMouseX    = (float)x;
    lastMouseY    = (float)y;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    camera.yaw   += xoffset;
    camera.pitch += yoffset;

    if (camera.pitch > 89.0f)  camera.pitch = 89.0f;
    if (camera.pitch < -89.0f) camera.pitch = -89.0f;
}

void idleCallback() {
    static int prevTime = glutGet(GLUT_ELAPSED_TIME);
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    int elapsed = currentTime - prevTime;
    prevTime = currentTime;
    deltaTime = elapsed / 1000.0f;

    updateGame(deltaTime);
    glutPostRedisplay();
}

// -----------------------------------------
// OpenGL/GLUT initialization
// -----------------------------------------

void initGL() {
    FILE *f = fopen("boy_02.obj", "r");
    if (!f) printf("FILE NOT FOUND AT RUNTIME!\n");
    else { printf("FILE FOUND!\n"); fclose(f); }

    
    playerModel.load("boy_02.obj");
   // <-- your filename

    
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

    setupLights();

    GLfloat mat_ambient[]  = {0.3f, 0.3f, 0.3f, 1.0f};
    GLfloat mat_diffuse[]  = {0.8f, 0.8f, 0.8f, 1.0f};
    GLfloat mat_specular[] = {0.9f, 0.9f, 0.9f, 1.0f};
    GLfloat mat_shininess  = 50.0f;
    glMaterialfv(GL_FRONT, GL_AMBIENT,   mat_ambient);
    glMaterialfv(GL_FRONT, GL_DIFFUSE,   mat_diffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR,  mat_specular);
    glMaterialf (GL_FRONT, GL_SHININESS, mat_shininess);

    glEnable(GL_TEXTURE_2D);

    texGround1 = loadBMPTexture("ground1.bmp");
    texGround2 = loadBMPTexture("ground2.bmp");
    texWall1   = loadBMPTexture("wall1.bmp");
    texWall2   = loadBMPTexture("wall2.bmp");

    buildLevel(currentLevel);
    if (!playerModel.load("Assets/Models/boy_02.obj")) {
        printf("MODEL FAILED TO LOAD!!\n");
    } else {
        printf("MODEL LOADED SUCCESSFULLY!\n");
    }

}

// -----------------------------------------
// main
// -----------------------------------------

int main(int argc, char **argv) {
    std::memset(keyStates, 0, sizeof(keyStates));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("DMET 502 - 3D Game Project");

    initGL();

    glutDisplayFunc(displayCallback);
    glutReshapeFunc(reshapeCallback);
    glutKeyboardFunc(keyboardDownCallback);
    glutKeyboardUpFunc(keyboardUpCallback);
    glutPassiveMotionFunc(mouseMotionCallback);
    glutIdleFunc(idleCallback);

    glutSetCursor(GLUT_CURSOR_NONE); // optional

    glutMainLoop();
    return 0;
}
