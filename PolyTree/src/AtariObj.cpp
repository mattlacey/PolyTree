#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

AtariObj::AtariObj(char* filename)
{
	o = loadObj(filename);

}

void AtariObj::SetupBuffers()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, o.vertCount * sizeof(V3), o.verts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, o.indexCount * sizeof(long), o.indices, GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V3), (void*)0);

    glBindVertexArray(0);
}

void AtariObj::Render()
{
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, o.faceCount);
}
