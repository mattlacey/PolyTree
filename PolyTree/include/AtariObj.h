#pragma once

#include <vector>

extern "C"
{
	#include "Obj.h"
}

typedef struct fV3
{
	union
	{
		float v[3];
		struct
		{
			float x;
			float y;
			float z;
		};
	};
} fV3;

class AtariObj
{
public:
	Obj o;
	std::vector<fV3>* fpVerts;
	
	AtariObj(char* filename);
	void Render();
	void GenerateTree();
	void WriteTree(char* filename);

private:
	unsigned int VAO, VBO, EBO;
	void GenerateNode(ObjNode* node, std::vector<ObjFace>* pFaces);
	void WriteNode(FILE* pFile, ObjNode* pNode);
	void SetupBuffers();
	bool IsConvex(std::vector<ObjFace> polySoup, float fudge);
	fV3 GetFaceAverage(ObjFace f);
	fV3 GetFaceNormal(ObjFace f);
	float Dot(fV3 v1, fV3 v2);
	fV3 Cross(fV3 v1, fV3 v2);
	fV3 Sub(fV3 v1, fV3 v2);
	float Length(fV3 v);
	fV3 Normalize(fV3 v);
};
