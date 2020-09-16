#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vector>
#include <algorithm>

#include <intrin.h>

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
    fpVerts = new std::vector<fV3>();
    fpVerts->reserve(o.vertCount);

    for (int i = 0; i < o.vertCount; i++)
    {
        fV3 vert;
        vert.x = (o.verts[i].x / 65536.0f);
        vert.y = (o.verts[i].y / 65536.0f);
        vert.z = (o.verts[i].z / 65536.0f);
        fpVerts->push_back(vert);
    }

    glBufferData(GL_ARRAY_BUFFER, o.vertCount * sizeof(float) * 3, fpVerts->data()[0].v, GL_STATIC_DRAW);

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

    ObjFace* faces = (ObjFace*)o.indices;

    for (int i = 0; i < o.faceCount; i++)
    {
        pFaces->push_back(faces[i]);
    }
    std::random_shuffle(pFaces->begin(), pFaces->end());

    GenerateNode(o.pRootNode, pFaces);
}

void AtariObj::WriteTree(char* filename)
{
    FILE* f = fopen(filename, "wb+");
    char fileType[4] = "LTO";
    unsigned char fileVersion = 1;
    unsigned long temp;

    if (!f)
    {
        return;
    }

    fwrite(fileType, 1, 3, f);
    fwrite(&fileVersion, 1, 1, f);

    temp = _byteswap_ulong(o.vertCount);
    fwrite(&temp, sizeof(long), 1, f);

    for (int i = 0; i < o.vertCount; i++)
    {
        temp = _byteswap_ulong(o.verts[i].x);
		fwrite(&temp, sizeof(unsigned long), 1, f);
        temp = _byteswap_ulong(o.verts[i].y);
		fwrite(&temp, sizeof(unsigned long), 1, f);
        temp = _byteswap_ulong(o.verts[i].z);
        fwrite(&temp, sizeof(unsigned long), 1, f);
    }

    WriteNode(f, o.pRootNode);

    fclose(f);
}

void AtariObj::WriteNode(FILE* pFile, ObjNode* pNode)
{
    unsigned long temp;
    if (pNode->pPart)
    {
        temp = _byteswap_ulong(pNode->pPart->faceCount);
        fwrite(&temp, sizeof(unsigned long), 1, pFile);

        for (long i = 0; i < pNode->pPart->faceCount; i++)
        {
            temp = _byteswap_ulong(pNode->pPart->faces[i].v1);
			fwrite(&temp, sizeof(unsigned long), 1, pFile);
            temp = _byteswap_ulong(pNode->pPart->faces[i].v2);
			fwrite(&temp, sizeof(unsigned long), 1, pFile);
            temp = _byteswap_ulong(pNode->pPart->faces[i].v3);
			fwrite(&temp, sizeof(unsigned long), 1, pFile);
        }
    }
    else
    {
        temp = 0;
        fwrite(&temp, sizeof(unsigned long), 1, pFile);

        temp = _byteswap_ulong(pNode->hyperplane.orientation);
        fwrite(&temp, sizeof(unsigned long), 1, pFile);
        temp = _byteswap_ulong(pNode->hyperplane.distance);
        fwrite(&temp, sizeof(unsigned long), 1, pFile);

        WriteNode(pFile, pNode->pLeft);
        WriteNode(pFile, pNode->pRight);
    }
}

void AtariObj::GenerateNode(ObjNode* node, std::vector<ObjFace>* pFaces)
{
    // if we're down to one face (later, no intersecting faces) stop
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

    // try out different plane options and find a reasonable balance (left/right of around 3:1 tops)
    // also need to try and minimise the number of polys that get split 
    long bestVert = -1;
    int bestAxis = -1;
    float ratio = 999999.9f;
    int bestScore = 99999999;
    int face = std::rand() % pFaces->size(); // random enough for this

    fV3* verts = fpVerts->data();

    // for each vert in the chosen face
    for (int i = 0; i < 3; i++)
    {
        long vert = ((long*)pFaces->data())[face * 3 + i];

        // we test each axis as a partitioning plane
        // 0 for x, 1 for y, 2 for z - same type used in the ObjPlane struct
        for (int j = 0; j < 3; j++)
        {
            float axisOffset = verts[vert].v[j];
			int score = 0;
            int splitCount = 0;

            // sift faces to left and right based on verts being greater
            // or less than the offset
            for (int k = 0; k < pFaces->size(); k++)
            {
                ObjFace face = (*pFaces)[k];

                if (verts[face.v1].v[j] <= axisOffset
                    && verts[face.v2].v[j] <= axisOffset
                    && verts[face.v3].v[j] <= axisOffset)
                {
                    score++;
                }
                else if (verts[face.v1].v[j] > axisOffset
                    && verts[face.v2].v[j] > axisOffset
                    && verts[face.v3].v[j] > axisOffset)
                {
                    score--;
                }
                else
                {
                    splitCount++;
                }
            }

            int finalScore = abs(score) * abs(score) + (splitCount * splitCount);
            
            if (finalScore < bestScore)
            {
                bestScore = finalScore;
                bestAxis = j;
                bestVert = vert;
            }
        }
    }

    // once the best score is found, go ahead and divide up the faces, splitting where necessary
    float bestAxisOffset = fpVerts->data()[bestVert].v[bestAxis];
    for (int k = 0; k < pFaces->size(); k++)
    {
        ObjFace face = (*pFaces)[k];

        if (verts[face.v1].v[bestAxis] <= bestAxisOffset
            && verts[face.v2].v[bestAxis] <= bestAxisOffset
            && verts[face.v3].v[bestAxis] <= bestAxisOffset)
        {
            pLeftFaces->push_back((*pFaces)[k]);
        }
        else if (verts[face.v1].v[bestAxis] > bestAxisOffset
            && verts[face.v2].v[bestAxis] > bestAxisOffset
            && verts[face.v3].v[bestAxis] > bestAxisOffset)
        {
            pRightFaces->push_back((*pFaces)[k]);
        }
        else
        {
            // grab the 3 verts
            // work out which side each is on
            // find intersections
            // record the new faces and push the new verts into the vector
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

void getIntersection(float *v1, float *v2, int axis)
{
    return;
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
