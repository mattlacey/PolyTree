#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vector>
#include <algorithm>

AtariObj::AtariObj(char* filename)
{
	o = loadObj(filename);
    fpVerts = NULL;
    SetupBuffers();
}

void AtariObj::SetupBuffers()
{
    if (fpVerts)
    {
        return;
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Atari verts are in 16.16 fixed point format, so we need to convert them here to the correct format for OpenGL
    fpVerts = (float*)malloc(sizeof(float) * 3 * o.vertCount);

    for (int i = 0; i < o.vertCount; i++)
    {
        fpVerts[i * 3 + 0] = (o.verts[i].x / 65536.0f);
        fpVerts[i * 3 + 1] = (o.verts[i].y / 65536.0f);
        fpVerts[i * 3 + 2] = (o.verts[i].z / 65536.0f);
    }

    glBufferData(GL_ARRAY_BUFFER, o.vertCount * sizeof(float) * 3, fpVerts, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, o.indexCount * sizeof(long), o.indices, GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
    glBindVertexArray(0);
}

void AtariObj::GenerateTree()
{
    o.pRootNode = (ObjNode*)malloc(sizeof(ObjNode));
    std::vector<ObjFace> faces(o.faceCount);
    std::random_shuffle(faces.begin(), faces.end());

    GenerateNode(o.pRootNode, faces);
}

void AtariObj::GenerateNode(ObjNode* node, std::vector<ObjFace> faces)
{
    // if no we're down to one face (later, no intersecting faces) stop
    if (faces.size() <= 1)
    {
        node->pPart = (ObjPart*)malloc(sizeof(ObjPart));
        node->pPart->faces = &faces[0];
        node->pPart->faceCount = (long)faces.size();
        return;
    }

    std::vector<ObjFace> left;
    std::vector<ObjFace> right;

    // try out different plane options and find a reasonable split ratio (3:1)
    long bestVert = -1;
    float ratio = 999999.9f;

    // for (int i = 0; i < 3; i++)
    int i = 0;
    {
        long vert = ((long*)faces.data())[i];

        // 0 for x, 1 for y, 2 for z - same type used in the ObjPlane struct
        //for (int j = 0; j < 3; j++)
        int j = 0;
        {
            float offset = fpVerts[vert * 3 + j];

            // sift faces to left and right based on verts being greater
            // or less than the offset

            // if ratio is closer to 1 then update bestVert and best ratio
        }
    }

    // once a reasonable split is found then alloc a new node for each size,
    // wash, rinse, repeat?
}

bool AtariObj::FacesIntersect(ObjFace* face1, ObjFace* face2)
{
    return false;
}

void AtariObj::Render()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, o.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
