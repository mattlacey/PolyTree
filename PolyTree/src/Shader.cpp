#include "Shader.h"

#include <glad/glad.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>


Shader::Shader(const char* vPath, const char* fPath)
{
	std::ifstream file;
	std::stringstream fStream;
	std::stringstream vStream;
	std::string fCodeString;
	std::string vCodeString;

	unsigned int vertexId, fragmentId;
	int success;

	programId = -1;

	try
	{
		file.open(vPath);
		vStream << file.rdbuf();
		file.close();
		vCodeString = vStream.str();
		const char * vCode = (const char*)vCodeString.c_str();

		file.open(fPath);
		fStream << file.rdbuf();
		file.close();
		fCodeString = fStream.str();
		const char* fCode = (const char*)fCodeString.c_str();

		vertexId = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vertexId, 1, &vCode, NULL);
		glCompileShader(vertexId);

		glGetShaderiv(vertexId, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			glGetShaderInfoLog(vertexId, 1024, NULL, errorLog);
			std::cout << "Error compiling vertex shader\n" << errorLog << std::endl;
			return;
		}
		
		fragmentId = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragmentId, 1, &fCode, NULL);
		glCompileShader(fragmentId);

		glGetShaderiv(fragmentId, GL_COMPILE_STATUS, &success);

		if (!success)
		{
			glGetShaderInfoLog(fragmentId, 1024, NULL, errorLog);
			std::cout << "Error compiling fragment shader\n" << errorLog << std::endl;
			return;
		}

		programId = glCreateProgram();
		glAttachShader(programId, vertexId);
		glAttachShader(programId, fragmentId);
		glLinkProgram(programId);

		glGetShaderiv(programId, GL_LINK_STATUS, &success);

		if (!success)
		{
			glGetProgramInfoLog(programId, 1024, NULL, errorLog);
			std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << errorLog << std::endl;
			return;
		}

		glDeleteShader(vertexId);
		glDeleteShader(fragmentId);

		sprintf(errorLog, "Shaders compiled successfully.");
	}
	catch (std::ifstream::failure e)
	{
		std::cout << "Error reading shader file" << std::endl;
	}
}

void Shader::Use()
{
	glUseProgram(programId);
}

