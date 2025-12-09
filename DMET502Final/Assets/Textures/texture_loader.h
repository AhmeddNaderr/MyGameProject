#pragma once
#include <GLUT/glut.h>
#include <stdio.h>

GLuint loadBMP(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) { printf("Failed to open %s\n", filename); return 0; }

    unsigned char header[54];
    fread(header, 1, 54, file);

    unsigned int dataPos   = *(int*)&(header[0x0A]);
    unsigned int imageSize = *(int*)&(header[0x22]);
    unsigned int width     = *(int*)&(header[0x12]);
    unsigned int height    = *(int*)&(header[0x16]);

    if (imageSize == 0) imageSize = width * height * 3;
    if (dataPos == 0) dataPos = 54;

    unsigned char* data = new unsigned char[imageSize];
    fread(data, 1, imageSize, file);
    fclose(file);

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height,
                 0, GL_BGR, GL_UNSIGNED_BYTE, data);

    delete[] data;
    return texID;
}
//
//  texture_loader.h
//  DMET502Final
//
//  Created by omar on 08/12/2025.
//

