#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <math.h>

#include <intrin.h>

AtariObj::AtariObj(char* filename)
{
    VAO = VBO = EBO = -1;
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
    // and before we try and split the remaining faces, need to test
    // to see if we've got a convex mesh or not, if so we can stop
    if (pFaces->size() <= 1 || IsConvex(*pFaces))
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
            // or less than the offset - on the line doesn't matter either
            // way hence both <= and >=
            for (int k = 0; k < pFaces->size(); k++)
            {
                ObjFace face = (*pFaces)[k];

                if (verts[face.v1].v[j] <= axisOffset
                    && verts[face.v2].v[j] <= axisOffset
                    && verts[face.v3].v[j] <= axisOffset)
                {
                    score++;
                }
                else if (verts[face.v1].v[j] >= axisOffset
                    && verts[face.v2].v[j] >= axisOffset
                    && verts[face.v3].v[j] >= axisOffset)
                {
                    score--;
                }
                else
                {
                    splitCount++;
                }
            }

            int finalScore = 2 * (abs(score) * abs(score)) + (splitCount * splitCount);
            
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
        else if (verts[face.v1].v[bestAxis] >= bestAxisOffset
            && verts[face.v2].v[bestAxis] >= bestAxisOffset
            && verts[face.v3].v[bestAxis] >= bestAxisOffset)
        {
            pRightFaces->push_back((*pFaces)[k]);
        }
        else
        {
            // grab the 3 verts
            // work out which side each is on
            // find intersections
            // record the new faces and push the new verts into the vector
            fV3 faceVerts[3];
            int faceIndices[3];
            fV3 p, q;
            int i0 = 0, i1 = 1, i2 = 2;
           
            faceIndices[i0] = face.v1;
            faceIndices[i1] = face.v2;
            faceIndices[i2] = face.v3;

            faceVerts[i0] = verts[face.v1];
            faceVerts[i1] = verts[face.v2];
            faceVerts[i2] = verts[face.v3];

            // faces are wound 0, 1, 2, we'll set up vert 0 to be one side
            // of the device, and 1 and 2 the other, we 'rotate' them to
            // get to that setup so that we're not screwing with the winding
            // then work out where the cut is and calculate new faces accordingly
            // p will be the point between 0 & 1, q will be the point between 0 & 2

            if (faceVerts[0].v[bestAxis] <= bestAxisOffset)
            {
                if (faceVerts[1].v[bestAxis] <= bestAxisOffset)
                {
                    // 1 is on the same side as 0, 'rotate' opposite to winding
                    i0 = 1;
                    i1 = 2;
                    i2 = 0;
                 
                }
                else if(faceVerts[2].v[bestAxis] <= bestAxisOffset)
                {
                    // 2 is on the same side as 0, 'rotate' with winding
                    i0 = 2;
                    i1 = 0;
                    i2 = 1;
                }
            }
            else
            {
                if (faceVerts[1].v[bestAxis] >= bestAxisOffset)
                {
                    // 1 is on the same side as 0
                    i0 = 1;
                    i1 = 2;
                    i2 = 0;

                }
                else if(faceVerts[2].v[bestAxis] >= bestAxisOffset)
                {
                    // 2 is on the same side as 0
                    i0 = 2;
                    i1 = 0;
                    i2 = 1;
                }
            }

            float dp = (bestAxisOffset - faceVerts[i0].v[bestAxis]) / (faceVerts[i1].v[bestAxis] - faceVerts[i0].v[bestAxis]);
            float dq = (bestAxisOffset - faceVerts[i0].v[bestAxis]) / (faceVerts[i2].v[bestAxis] - faceVerts[i0].v[bestAxis]);

            // if either dp or dq are 1 then we've got a special case scenario where we'll only have two triangles after splitting

            p.x = faceVerts[i0].x + dp * (faceVerts[i1].x - faceVerts[i0].x);
            p.y = faceVerts[i0].y + dp * (faceVerts[i1].y - faceVerts[i0].y);
            p.z = faceVerts[i0].z + dp * (faceVerts[i1].z - faceVerts[i0].z);

            q.x = faceVerts[i0].x + dq * (faceVerts[i2].x - faceVerts[i0].x);
            q.y = faceVerts[i0].y + dq * (faceVerts[i2].y - faceVerts[i0].y);
            q.z = faceVerts[i0].z + dq * (faceVerts[i2].z - faceVerts[i0].z);

            // new tris are 0pq, p12, p2q - push the verts verts so that we have
            // indices for them and then we can push faces onto the appropriate side
            fpVerts->push_back(p);
            fpVerts->push_back(q);

            int ip = (int)fpVerts->size() - 2;
            int iq = (int)ip + 1;

            // optimise this for the case where a poly is split perfectly into two triangles

            if (faceVerts[0].v[bestAxis] <= bestAxisOffset)
            {
                face.v1 = faceIndices[i0];
                face.v2 = ip;
                face.v3 = iq;
                pLeftFaces->push_back(face);

                face.v1 = ip;
                face.v2 = faceIndices[i1];
                face.v3 = faceIndices[i2];
                pRightFaces->push_back(face);

                face.v2 = faceIndices[i2];
                face.v3 = iq;
                pRightFaces->push_back(face);
            }
            else
            {

                face.v1 = faceIndices[i0];
                face.v2 = ip;
                face.v3 = iq;
                pRightFaces->push_back(face);

                face.v1 = ip;
                face.v2 = faceIndices[i1];
                face.v3 = faceIndices[i2];
                pLeftFaces->push_back(face);

                face.v2 = faceIndices[i2];
                face.v3 = iq;
                pLeftFaces->push_back(face);
            }
        }
    }

    node->pPart = NULL;
    node->hyperplane.distance = (fx32)(FX_ONE * bestAxisOffset);
    node->hyperplane.orientation = bestAxis;

    if (pLeftFaces->size())
    {
        node->pLeft = (ObjNode*)malloc(sizeof(ObjNode));
        node->pLeft->pLeft = NULL;
        GenerateNode(node->pLeft, pLeftFaces);
    }
    else
    {
        node->pLeft = NULL;
    }

    if (pRightFaces->size())
    {
        node->pRight = (ObjNode*)malloc(sizeof(ObjNode));
        node->pRight->pRight = NULL;
        GenerateNode(node->pRight, pRightFaces);
    }
    else
    {
        node->pRight = NULL;
    }
}

bool AtariObj::IsConvex(std::vector<ObjFace> polySoup)
{
    /*
        For each poly, check the verts of all the others, if they're all behind it, i.e. the dot
        product from the poly average to the verts is < 0,  then we're convex and there should
        be no rendering issues involved
    */
    // iterate foo: inserting into bar
    for (std::vector<ObjFace>::size_type i = 0; i < polySoup.size(); i++)
    {
        fV3 faceAvg = GetFaceAverage(polySoup[i]);
        fV3 faceNormal = GetFaceNormal(polySoup[i]);

        for (std::vector<ObjFace>::size_type j = 0; j < polySoup.size(); j++)
        {
            if (i == j)
            {
                continue;
            }

            // could loop using a pointer but using named components will be more robust
            fV3 v, v2, v3;
            float dp;

            v = fpVerts->at(polySoup[j].v1);

            v2 = Sub(v, faceAvg);
            v3 = Normalize(v2);
            dp = Dot(v3, faceNormal);
            if (dp > 0.01f)
                return false;

            v = fpVerts->at(polySoup[j].v2);
            v2 = Sub(v, faceAvg);
            v3 = Normalize(v2);
            dp = Dot(v3, faceNormal);
            if (dp > 0.01f)
                return false;

            v = fpVerts->at(polySoup[j].v3);
            v2 = Sub(v, faceAvg);
            v3 = Normalize(v2);
            dp = Dot(v3, faceNormal);
            if (dp > 0.01f)
                return false;
           
        }
    }

    return true;
}

fV3 AtariObj::GetFaceAverage(ObjFace f)
{
    fV3 avg;
    fV3 v1 = fpVerts->at(f.v1);
    fV3 v2 = fpVerts->at(f.v2);
    fV3 v3 = fpVerts->at(f.v3);

    avg.x = (v1.x + v2.x + v3.x) / 3.0f;
    avg.y = (v1.y + v2.y + v3.y) / 3.0f;
    avg.z = (v1.z + v2.z + v3.z) / 3.0f;

    return avg;
}

fV3 AtariObj::GetFaceNormal(ObjFace f)
{
    fV3 v1 = fpVerts->at(f.v1);
    fV3 v2 = fpVerts->at(f.v2);
    fV3 v3 = fpVerts->at(f.v3);

    return Cross(Normalize(Sub(v2, v1)), Normalize(Sub(v3, v1)));
}

float AtariObj::Dot(fV3 v1, fV3 v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

fV3 AtariObj::Cross(fV3 v1, fV3 v2)
{
    fV3 cp;

    cp.x = v1.y * v2.z - v1.z * v2.y;
    cp.y = v1.z * v2.x - v1.x * v2.z;
    cp.z = v1.x * v2.y - v1.y * v2.x;

    return Normalize(cp);
}

fV3 AtariObj::Sub(fV3 v1, fV3 v2)
{
    fV3 result;

    result.x = v1.x - v2.x;
    result.y = v1.y - v2.y;
    result.z = v1.z - v2.z;

    return result;
}

float AtariObj::Length(fV3 v)
{
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

fV3 AtariObj::Normalize(fV3 v)
{
    float len = Length(v);

    v.x /= len;
    v.y /= len;
    v.z /= len;

    return v;
}

void AtariObj::Render()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, o.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
