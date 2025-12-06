#ifndef OBJMODEL_H
#define OBJMODEL_H

#include <vector>
#include <string>
#include <GLUT/glut.h>

class ObjModel {
public:
    bool load(const std::string &path);
    void draw() const;

private:
    struct Vertex { float x, y, z; };
    struct TexCoord { float u, v; };
    struct Normal { float nx, ny, nz; };

    struct Face {
        std::vector<int> vIdx;
        std::vector<int> tIdx;
        std::vector<int> nIdx;
    };

    std::vector<Vertex>  vertices;
    std::vector<TexCoord> texcoords;
    std::vector<Normal>  normals;
    std::vector<Face>    faces;
};

#endif
