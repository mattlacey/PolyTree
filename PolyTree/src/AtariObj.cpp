#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vector>
#include <algorithm>

AtariObj::AtariObj(char* filename)
{
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename || !_strcmpi(dot, ".obj"))
    {
        o = loadObj(filename);
		fpVerts = NULL;
		SetupBuffers();
		GenerateTree();
    }
    else
    {
        o = loadTree(filename);
    }
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
    std::vector<ObjFace>* pFaces = new std::vector<ObjFace>();

    for (int i = 0; i < o.faceCount; i++)
    {
        pFaces->push_back(o.faces[i]);
    }
    std::random_shuffle(pFaces->begin(), pFaces->end());

    GenerateNode(o.pRootNode, pFaces);
}

void AtariObj::WriteTree(char* filename)
{
    FILE* f = fopen(filename, "wb+");
    char fileType[4] = "LTO";
    unsigned char fileVersion = 1;

    if (!f)
    {
        return;
    }

    fwrite(fileType, 1, 3, f);
    fwrite(&fileVersion, 1, 1, f);
    fwrite(&o.vertCount, sizeof(long), 1, f);
    fwrite(&o.verts[0], sizeof(V3), o.vertCount, f);

    WriteNode(f, o.pRootNode);

    fclose(f);
}

void AtariObj::WriteNode(FILE* pFile, ObjNode* pNode)
{
    if (pNode->pPart)
    {
        unsigned long faceCount = pNode->pPart->faceCount;
        fwrite(&faceCount, sizeof(unsigned long), 1, pFile);
        fwrite(pNode->pPart->faces, sizeof(ObjFace), pNode->pPart->faceCount, pFile);
    }
    else
    {
        unsigned long marker = BRANCH_NODE;
        fwrite(&marker, sizeof(unsigned long), 1, pFile);

        fwrite(&pNode->hyperplane, sizeof(BSPPlane), 1, pFile);
        WriteNode(pFile, pNode->pLeft);
        WriteNode(pFile, pNode->pRight);
    }
}

void AtariObj::GenerateNode(ObjNode* node, std::vector<ObjFace>* pFaces)
{
    // if no we're down to one face (later, no intersecting faces) stop
    if (pFaces->size() <= 1)
    {
        node->pPart = (ObjPart*)malloc(sizeof(ObjPart));
        node->pPart->faces = &(*pFaces)[0];
        node->pPart->faceCount = (long)pFaces->size();
        node->pLeft = NULL;
        node->pRight = NULL;
        return;
    }

    std::vector<ObjFace>* pLeftFaces = new std::vector<ObjFace>();
    std::vector<ObjFace>* pRightFaces = new std::vector<ObjFace>();

    // try out different plane options and find a reasonable split ratio (3:1)
    long bestVert = -1;
    int bestAxis = -1;
    float ratio = 999999.9f;
    int bestScore = 99999999;
    int face = std::rand() % pFaces->size(); // random enough for this

    for (int i = 0; i < 3; i++)
    {
        long vert = ((long*)pFaces->data())[face * 3 + i];

        // 0 for x, 1 for y, 2 for z - same type used in the ObjPlane struct
        for (int j = 0; j < 3; j++)
        {
            float axisOffset = fpVerts[vert * 3 + j];
			int score = 0;

            // sift faces to left and right based on verts being greater
            // or less than the offset
            for (int k = 0; k < pFaces->size(); k++)
            {
                // dumb for now - just take the average
                float average = (fpVerts[(*pFaces)[k].v1 * 3 + j] + fpVerts[(*pFaces)[k].v2 * 3 + j] + fpVerts[(*pFaces)[k].v3 * 3 + j]) / 3.0f;
               
                if (average <= axisOffset)
                {
                    score++;
                }
                else
                {
                    score--;
                }
            }
            
            if (abs(score) < bestScore)
            {
                bestScore = abs(score);
                bestAxis = j;
                bestVert = vert;
            }
        }
    }

    // once a reasonable split is found then alloc a new node for each size,
    // wash, rinse, repeat?
    float bestAxisOffset = fpVerts[bestVert * 3 + bestAxis];
    for (int k = 0; k < pFaces->size(); k++)
    {
        float average = (fpVerts[(*pFaces)[k].v1 * 3 + bestAxis] + fpVerts[(*pFaces)[k].v2 * 3 + bestAxis] + fpVerts[(*pFaces)[k].v3 * 3 + bestAxis]) / 3.0f;

        if (average <= bestAxisOffset)
        {
            pLeftFaces->push_back((*pFaces)[k]);
        }
        else
        {
            pRightFaces->push_back((*pFaces)[k]);
        }
    }

    node->pPart = NULL;
    node->hyperplane.distance = (fx32)(FX_ONE * bestAxisOffset);
    node->hyperplane.orientation = bestAxis;
    node->pLeft = (ObjNode*)malloc(sizeof(ObjNode));
    node->pRight = (ObjNode*)malloc(sizeof(ObjNode));

    node->pLeft->pLeft = NULL;
    node->pRight->pRight = NULL;

    // if all faces have fallen on one side, then there's no way to split them so we're done
    // on this branch
    if (pFaces->size() == pLeftFaces->size() || pFaces->size() == pRightFaces->size())
    {
        node->pPart = (ObjPart*)malloc(sizeof(ObjPart));
        node->pPart->faces = (pFaces->size() == pLeftFaces->size() ? &(*pLeftFaces)[0] : &(*pRightFaces)[0]);
        node->pPart->faceCount = (long)pFaces->size();
        return;
    }
    else
    {
        GenerateNode(node->pLeft, pLeftFaces);
        GenerateNode(node->pRight, pRightFaces);
    }
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
