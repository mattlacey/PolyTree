#include "AtariObj.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <stdlib.h>
#include <vector>
#include <set>
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

    temp = _byteswap_ulong(fpVerts->size());
    fwrite(&temp, sizeof(long), 1, f);

    for (unsigned long i = 0; i < fpVerts->size(); i++)
    {
        fV3 v = fpVerts->at(i);

        temp = _byteswap_ulong((unsigned long)(65536 * v.x));
        fwrite(&temp, sizeof(unsigned long), 1, f);
        temp = _byteswap_ulong((unsigned long)(65536 * v.y));
        fwrite(&temp, sizeof(unsigned long), 1, f);
        temp = _byteswap_ulong((unsigned long)(65536 * v.z));
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
    if (pFaces->size() <= 1 || IsConvex(*pFaces, 0.1f))
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

    /* Notes from Doug
    
    For deciding number of planes to try:

    - use something like min_tries+(log(1+group_size)) as a guide for the number of plane trials in a group, where min_tries is a min number
        to try for any group (but never more than the actual group_size of course! make sure it is capped). this will allow more planes to be
        tried for larger groups, but avoid exponential cost and wasted trials in an already diverse pool. choosing a good plane is more important
        in smaller groups than it is in larger ones because there are many small groups at the tree leaves and these need to be 'evaporated' very efficiently.

    For scoring:

    - calculate partition ratio of a:b
         pr = min(a,b) / max(a,b)
        yields range 0+e -> 1.0 (can get close to zero, but avoiding zero)
        where e.g.: 1.0 = perfectly balanced, 0.01 = poorly balanced, 100:1 ratio
        better to reduce scoring sensitivity to medium-good balance, high sensitivity to poor-medium balance.
        best done with a power/compression curve on range 0-1 e.g.
         score = pow(pr,0.35-0.85)
        compresses values >0.25 nearer 1.0
        so:
             balance_ratio = pow((min(a,b)/max(a,b)), 0.5);

    -  calculate ratio of [S] to the rest
        bit more tricky - using same ratio calculation for splits, a value near 1.0 would be a bad outcome so it needs to be inverted
            i.e. equally balanced splits to non-splits would be an inefficient output. a low ratio of splits to non-splits is preferred
             split = count [S]
             nonsplit = count [F] + count [B]
             raw_split_ratio = min(split,nonsplit)/max(split,nonsplit)
            same range 0+e -> 1.0, but  where 1.0 = bad. (small ratios are range-compressed due to divide, so more sensitive near 1.0, less near 0.0)
             inverted_ratio = (1.0 - raw_split_ratio), now 1.0 = good, (but large ratios are compressed. i.e. more sensitive nearer 0.0, less near 1.0)
            since we care more about selections near the 'good' end of the range (near 1.0), we want to correct the curve with another pow()
             split_ratio = pow(inverted_ratio, 3.0)

    - combine the scores:
            - this takes some care. each score (splitting or balancing) might dominate at different stages of the tree.
            - near the tree root, picking decent planes is less important but creating lots of splits is also bad. so you want both to get a fair contribution,
                with perhaps balancing being dominant.
            - near the leaves, perhaps let the splitting score dominate more
        - multplying the two scores into a final score will effectively punish a plane

    */


    // try out different plane options and find a reasonable balance (left/right of around 3:1 tops)
    // also need to try and minimise the number of polys that get split 
    long bestVert = -1;
    int bestAxis = -1;
    float ratio = 999999.9f;
    float dp;
    float dq;
    int bestScore = 99999999;
    int bestSplit = 0;
    int face;
    int bestGroupingScore = 0;
    int faceAttempts = (pFaces->size() < 5 ? pFaces->size() : 5);
    float fudge = 0.1f;

    fV3* verts = fpVerts->data();
    std::set<int> testedFaces;

    while (faceAttempts--)
    {
        face = std::rand() % pFaces->size(); // random enough for this

        auto search = testedFaces.find(face);
        while (search != testedFaces.end())
        {
            search = testedFaces.find(face);
        }

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

                    if (verts[face.v1].v[j] <= axisOffset + fudge
                        && verts[face.v2].v[j] <= axisOffset + fudge
                        && verts[face.v3].v[j] <= axisOffset + fudge)
                    {
                        score++;
                    }
                    else if (verts[face.v1].v[j] >= axisOffset - fudge
                        && verts[face.v2].v[j] >= axisOffset - fudge
                        && verts[face.v3].v[j] >= axisOffset - fudge)
                    {
                        score--;
                    }
                    else
                    {
                        splitCount++;
                    }
                }

                int finalScore = 1 * (abs(score) * abs(score)) + (splitCount * splitCount);

                if (finalScore < bestScore)
                {
                    bestScore = finalScore;
                    bestGroupingScore = score;
                    bestAxis = j;
                    bestVert = vert;
                    bestSplit = splitCount;
                }
            }
        }
    }

    // check we managed to actually divide the group, if not we'll call it a day for now
    if (bestGroupingScore == pFaces->size() && bestSplit == 0)
    {
        node->pPart = (ObjPart*)malloc(sizeof(ObjPart));
        node->pPart->faces = &(*pFaces)[0];
        node->pPart->faceCount = (long)pFaces->size();
        node->pLeft = NULL;
        node->pRight = NULL;
        return;
    }

    // once the best score is found, go ahead and divide up the faces, splitting where necessary
    float bestAxisOffset = fpVerts->data()[bestVert].v[bestAxis];
    for (int k = 0; k < pFaces->size(); k++)
    {
        ObjFace face = (*pFaces)[k];

        if (verts[face.v1].v[bestAxis] <= bestAxisOffset + fudge
            && verts[face.v2].v[bestAxis] <= bestAxisOffset + fudge
            && verts[face.v3].v[bestAxis] <= bestAxisOffset + fudge)
        {
            pLeftFaces->push_back((*pFaces)[k]);
        }
        else if (verts[face.v1].v[bestAxis] >= bestAxisOffset - fudge
            && verts[face.v2].v[bestAxis] >= bestAxisOffset - fudge
            && verts[face.v3].v[bestAxis] >= bestAxisOffset - fudge)
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


            // there are some bugs in this rotating that mean we're not actually splitting triangles resulting in one side of the tree being empty

            if (faceVerts[0].v[bestAxis] <= bestAxisOffset)
            {
                if (faceVerts[1].v[bestAxis] < bestAxisOffset)
                {
                    // 1 is on the same side as 0, 'rotate' opposite to winding
                    i0 = 2;
                    i1 = 0;
                    i2 = 1;
                 
                }
                else if(faceVerts[2].v[bestAxis] < bestAxisOffset)
                {
                    // 2 is on the same side as 0, 'rotate' with winding
                    i0 = 1;
                    i1 = 2;
                    i2 = 0;
                }
            }
            else
            {
                if (faceVerts[1].v[bestAxis] > bestAxisOffset)
                {
                    // 1 is on the same side as 0
                    i0 = 2;
                    i1 = 0;
                    i2 = 1;

                }
                else if(faceVerts[2].v[bestAxis] > bestAxisOffset)
                {
                    // 2 is on the same side as 0
                    i0 = 1;
                    i1 = 2;
                    i2 = 0;
                }
            }

            dp = (bestAxisOffset - faceVerts[i0].v[bestAxis]) / (faceVerts[i1].v[bestAxis] - faceVerts[i0].v[bestAxis]);
            dq = (bestAxisOffset - faceVerts[i0].v[bestAxis]) / (faceVerts[i2].v[bestAxis] - faceVerts[i0].v[bestAxis]);

            // if either dp or dq are 1 then we've got a special case scenario where we'll only have two triangles after splitting

            p.x = faceVerts[i0].x + dp * (faceVerts[i1].x - faceVerts[i0].x);
            p.y = faceVerts[i0].y + dp * (faceVerts[i1].y - faceVerts[i0].y);
            p.z = faceVerts[i0].z + dp * (faceVerts[i1].z - faceVerts[i0].z);

            q.x = faceVerts[i0].x + dq * (faceVerts[i2].x - faceVerts[i0].x);
            q.y = faceVerts[i0].y + dq * (faceVerts[i2].y - faceVerts[i0].y);
            q.z = faceVerts[i0].z + dq * (faceVerts[i2].z - faceVerts[i0].z);

            int ip = -1;
            int iq = -1;

            // new tris are 0pq, p12, p2q - first check to see if any existing verts
            // match, if so just use those indices, otherwise push on the back
            for (int i = 0; i < fpVerts->size(); i++)
            {
                if (verts[i].x == p.x && verts[i].y == p.y && verts[i].z == p.z)
                {
                    ip = i;
                    break;
                }
            }

            for (int i = 0; i < fpVerts->size(); i++)
            {
                if (verts[i].x == q.x && verts[i].y == q.y && verts[i].z == q.z)
                {
                    iq = i;
                    break;
                }
            }

            if (ip == -1)
            {
                fpVerts->push_back(p);
                ip = (int)fpVerts->size() - 1;
            }

            if (iq == -1)
            {
                fpVerts->push_back(q);
                iq = (int)fpVerts->size() - 1;
            }

            std::vector<ObjFace>* side1 = pLeftFaces;
            std::vector<ObjFace>* side2 = pRightFaces;

            if (faceVerts[0].v[bestAxis] > bestAxisOffset)
            {
                side1 = pRightFaces;
                side2 = pLeftFaces;
            }

            if (fabs(dp) >= 0.01f && fabs(dq) >= 0.01f)
            {
                face.v1 = faceIndices[i0];
                face.v2 = ip;
                face.v3 = iq;
                side1->push_back(face);
            }

            // if dp  is ~1.0f then it's at the second vert, so we skip this triangle
            if (fabs(1.0f - dp) > 0.01f)
            {
                face.v1 = ip;
                face.v2 = faceIndices[i1];
                face.v3 = faceIndices[i2];
                side2->push_back(face);
            }

            // likewise, if dq is ~1.0f then it's at the third vert, and we skip this one
            if (fabs(1.0f - dq) >= 0.01f)
            {
                face.v1 = ip;
                face.v2 = faceIndices[i2];
                face.v3 = iq;
                side2->push_back(face);
            }
        }
    }

    node->pPart = NULL;
    node->hyperplane.distance = (fx32)(FX_ONE * bestAxisOffset);
    node->hyperplane.orientation = bestAxis;

    // for now we're going to just lump some things together here, getting pairs of polys that should really
    // be counted as convex but aren't (see Debug2.obj)
    if (pLeftFaces->size() == 0 || pRightFaces->size() == 0)
    {
        node->pPart = (ObjPart*)malloc(sizeof(ObjPart));

        if (pLeftFaces->size())
        {
            node->pPart->faces = &(*pLeftFaces)[0];
            node->pPart->faceCount = (long)pLeftFaces->size();
        }
        else
        {
            node->pPart->faces = &(*pRightFaces)[0];
            node->pPart->faceCount = (long)pRightFaces->size();
        }

        node->pLeft = NULL;
        node->pRight = NULL;
        return;
    }

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

bool AtariObj::IsConvex(std::vector<ObjFace> polySoup, float fudge = 0.0f)
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
            if (dp > fudge)
                return false;

            v = fpVerts->at(polySoup[j].v2);
            v2 = Sub(v, faceAvg);
            v3 = Normalize(v2);
            dp = Dot(v3, faceNormal);
            if (dp > fudge)
                return false;

            v = fpVerts->at(polySoup[j].v3);
            v2 = Sub(v, faceAvg);
            v3 = Normalize(v2);
            dp = Dot(v3, faceNormal);
            if (dp > fudge)
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
