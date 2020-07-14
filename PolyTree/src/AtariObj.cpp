#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>

AtariObj::AtariObj(char* filename)
{
	o = loadObj(filename);
    SetupBuffers();
}

void AtariObj::SetupBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Atari verts are in 16.16 fixed point format, so we need to convert them here to the correct format for OpenGL
    float* fpVerts = (float*)malloc(sizeof(float) * 3 * o.vertCount);

    for (int i = 0; i < o.vertCount; i++)
    {
        fpVerts[i * 3 + 0] = (o.verts[i].x / 65536.0f) * 0.5f;
        fpVerts[i * 3 + 1] = (o.verts[i].y / 65536.0f) * 0.5f;
        fpVerts[i * 3 + 2] = (o.verts[i].z / 65536.0f) * 0.5f;
    }

    glBufferData(GL_ARRAY_BUFFER, o.vertCount * sizeof(float) * 3, fpVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, o.indexCount * sizeof(long), o.indices, GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glBindVertexArray(0);
}

void AtariObj::Render()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, o.faceCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
