#pragma once
class Shader
{
public:
	unsigned int programId;

	Shader(const char* vPath, const char* fPath);

	void Use();
};

