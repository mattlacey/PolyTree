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
	bool FacesIntersect(ObjFace* face1, ObjFace* face2);
};
