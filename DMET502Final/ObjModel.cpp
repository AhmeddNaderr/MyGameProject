#include "ObjModel.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool ObjModel::load(const std::string &path) {
    vertices.clear();
    texcoords.clear();
    normals.clear();
    faces.clear();

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open OBJ file: " << path << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            Vertex v;
            ss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        }
        else if (type == "vt") {
            TexCoord t;
            ss >> t.u >> t.v;
            texcoords.push_back(t);
        }
        else if (type == "vn") {
            Normal n;
            ss >> n.nx >> n.ny >> n.nz;
            normals.push_back(n);
        }
        else if (type == "f") {
            Face face;
            std::string vertData;

            while (ss >> vertData) {
                int v = 0, t = 0, n = 0;
                char c;

                std::stringstream vs(vertData);

                if (vertData.find("//") != std::string::npos) {
                    // format: v//n
                    vs >> v >> c >> c >> n;
                } else if (vertData.find("/") != std::string::npos) {
                    // format: v/t/n OR v/t
                    if (vertData.find("/") != vertData.rfind("/")) {
                        // v/t/n
                        vs >> v >> c >> t >> c >> n;
                    } else {
                        // v/t
                        vs >> v >> c >> t;
                    }
                } else {
                    // format: v
                    vs >> v;
                }

                face.vIdx.push_back(v - 1);
                if (t > 0) face.tIdx.push_back(t - 1);
                else face.tIdx.push_back(-1);
                if (n > 0) face.nIdx.push_back(n - 1);
                else face.nIdx.push_back(-1);
            }

            faces.push_back(face);
        }
    }

    std::cout << "Loaded OBJ: " << path << " ("
              << vertices.size() << " vertices, "
              << faces.size() << " faces)" << std::endl;

    return true;
}

void ObjModel::draw() const {
    for (const auto &f : faces) {
        glBegin(GL_POLYGON);
        for (size_t i = 0; i < f.vIdx.size(); i++) {
            if (f.nIdx[i] >= 0) {
                auto &n = normals[f.nIdx[i]];
                glNormal3f(n.nx, n.ny, n.nz);
            }
            if (f.tIdx[i] >= 0) {
                auto &t = texcoords[f.tIdx[i]];
                glTexCoord2f(t.u, t.v);
            }
            auto &v = vertices[f.vIdx[i]];
            glVertex3f(v.x, v.y, v.z);
        }
        glEnd();
    }
}
