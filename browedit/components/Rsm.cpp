#include "Rsm.h"

#include <browedit/util/FileIO.h>
#include <browedit/util/Util.h>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

Rsm::Rsm(const std::string& fileName)
{
	this->fileName = fileName;
	rootMesh = NULL;
	loaded = false;
	if (fileName.length() > 2 && fileName.substr(fileName.size() - 5) == ".rsm2")
		return;

	std::istream* rsmFile = util::FileIO::open(fileName);
	if (!rsmFile)
	{
		std::cerr << "RSM: Unable to open " << fileName << std::endl;
		return;
	}

	char header[4];
	rsmFile->read(header, 4);
	if (header[0] == 'G' && header[1] == 'R' && header[2] == 'G' && header[3] == 'M')
	{
		std::cerr<< "RSM: Unknown RSM header in file " << fileName << ", stopped loading" << std::endl;
		delete rsmFile;
		return;
	}

	//std::cout << "RSM: loading " << fileName << std::endl;
	
	rsmFile->read(reinterpret_cast<char*>(&version), sizeof(short));
	util::swapShort(version);
	rsmFile->read(reinterpret_cast<char*>(&animLen), sizeof(int));

	rsmFile->read(reinterpret_cast<char*>(&shadeType), sizeof(int));

	if (version >= 0x0104)
		alpha = rsmFile->get();
	else
		alpha = 0;
	rsmFile->read(unknown, 16);
	for (int i = 0; i < 16; i++)
		assert(unknown[i] == 0);

	int textureCount;
	rsmFile->read(reinterpret_cast<char*>(&textureCount), sizeof(int));;
	if (textureCount > 100 || textureCount < 0)
	{
		std::cerr << "Invalid textureCount, aborting" << std::endl;
		delete rsmFile;
		return;
	}

	for (int i = 0; i < textureCount; i++)
	{
		std::string textureFileName = util::FileIO::readString(rsmFile, 40);;
		textures.push_back(textureFileName);
	}

	std::string mainNodeName = util::FileIO::readString(rsmFile, 40);
	rsmFile->read(reinterpret_cast<char*>(&meshCount), sizeof(int));
	std::map<std::string, Mesh* > meshes;
	for (int i = 0; i < meshCount; i++)
	{
		Mesh* mesh = new Mesh(this, rsmFile);
		mesh->index = i;
		if (meshes.find(mesh->name) != meshes.end())
			mesh->name += "(duplicate)";
		if (mesh->name == "")
			mesh->name = "empty";
		meshes[mesh->name] = mesh;
	}

	if (meshes.find(mainNodeName) == meshes.end())
	{
		std::cerr << "RsmModel " << fileName << ": Could not locate root mesh" << std::endl;
		rootMesh = meshes.begin()->second;
	}
	else
		rootMesh = meshes[mainNodeName];

	rootMesh->parent = NULL;
	rootMesh->fetchChildren(meshes);


	updateMatrices();

	int numKeyFrames;
	rsmFile->read(reinterpret_cast<char*>(&numKeyFrames), sizeof(int));
	//assert(numKeyFrames == 0);

	int numVolumeBox;
	rsmFile->read(reinterpret_cast<char*>(&numVolumeBox), sizeof(int));
	if (numVolumeBox != 0)
		std::cerr << "WARNING! This model has " << numVolumeBox << " volume boxes!" << std::endl;

	loaded = true;


	delete rsmFile;
}


void Rsm::updateMatrices()
{
	bbmin = glm::vec3(999999, 999999, 999999);
	bbmax = glm::vec3(-999999, -999999, -9999999);
	dynamic_cast<Mesh*>(rootMesh)->setBoundingBox(bbmin, bbmax);
	bbrange = (bbmin + bbmax) / 2.0f;

	rootMesh->calcMatrix1(0);
	rootMesh->calcMatrix2();

	realbbmax = glm::vec3(-999999, -999999, -999999);
	realbbmin = glm::vec3(999999, 999999, 999999);
	glm::mat4 mat = glm::scale(glm::mat4(1.0f), glm::vec3(1, -1, 1));
	dynamic_cast<Mesh*>(rootMesh)->setBoundingBox2(mat, realbbmin, realbbmax);
	realbbrange = (realbbmax + realbbmin) / 2.0f;
	maxRange = glm::max(glm::max(realbbmax.x, -realbbmin.x), glm::max(glm::max(realbbmax.y, -realbbmin.y), glm::max(realbbmax.z, -realbbmin.z)));
}



Rsm::Mesh::Mesh(Rsm* model, std::istream* rsmFile)
{
	this->model = model;
	name = util::FileIO::readString(rsmFile, 40);
	parentName = util::FileIO::readString(rsmFile, 40);

	int textureCount;
	rsmFile->read(reinterpret_cast<char*>(&textureCount), sizeof(int));
	for (int i = 0; i < textureCount; i++)
	{
		int textureId;
		rsmFile->read(reinterpret_cast<char*>(&textureId), sizeof(int));
		textures.push_back(textureId);
	}

#if 1
	offset = glm::mat4(1.0f);
	rsmFile->read(reinterpret_cast<char*>(&offset[0][0]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[0][1]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[0][2]), sizeof(float));

	rsmFile->read(reinterpret_cast<char*>(&offset[1][0]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[1][1]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[1][2]), sizeof(float));

	rsmFile->read(reinterpret_cast<char*>(&offset[2][0]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[2][1]), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(&offset[2][2]), sizeof(float));
#else
	offset[0][0] = rsmFile->readFloat();//rotation
	offset[1][0] = rsmFile->readFloat();
	offset[2][0] = rsmFile->readFloat();

	offset[0][1] = rsmFile->readFloat();
	offset[1][1] = rsmFile->readFloat();
	offset[2][1] = rsmFile->readFloat();

	offset[0][2] = rsmFile->readFloat();
	offset[1][2] = rsmFile->readFloat();
	offset[2][2] = rsmFile->readFloat();
#endif

	rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(pos_)), sizeof(float) * 3);
	rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(pos)), sizeof(float) * 3);
	rsmFile->read(reinterpret_cast<char*>(&rotangle), sizeof(float));
	rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(rotaxis)), sizeof(float) * 3);
	rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(scale)), sizeof(float) * 3);

	int vertexCount;
	rsmFile->read(reinterpret_cast<char*>(&vertexCount), sizeof(int));
	vertices.resize(vertexCount);
	for (int i = 0; i < vertexCount; i++)
		rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(vertices[i])), sizeof(float) * 3);

	int texCoordCount;
	rsmFile->read(reinterpret_cast<char*>(&texCoordCount), sizeof(int));
	texCoords.resize(texCoordCount);
	for (int i = 0; i < texCoordCount; i++)
	{
		float todo = 0;
		if (model->version >= 0x0102)
		{
			rsmFile->read(reinterpret_cast<char*>(&todo), sizeof(float));
			assert(todo == 0);
		}
		rsmFile->read(reinterpret_cast<char*>(glm::value_ptr(texCoords[i])), sizeof(float) * 2);
		texCoords[i].y = texCoords[i].y;
	}

	int faceCount;
	rsmFile->read(reinterpret_cast<char*>(&faceCount), sizeof(int));
	faces.resize(faceCount);
	for (int i = 0; i < faceCount; i++)
	{
		Face* f = new Face();
		rsmFile->read(reinterpret_cast<char*>(f->vertexIds), sizeof(short) * 3);
		rsmFile->read(reinterpret_cast<char*>(f->texCoordIds), sizeof(short) * 3);
		rsmFile->read(reinterpret_cast<char*>(&f->texId), sizeof(short));
		rsmFile->read(reinterpret_cast<char*>(&f->padding), sizeof(short));

		rsmFile->read(reinterpret_cast<char*>(&f->twoSided), sizeof(int));
		rsmFile->read(reinterpret_cast<char*>(&f->smoothGroup), sizeof(int));

		faces[i] = f;

		f->normal = glm::normalize(glm::cross(vertices[f->vertexIds[1]] - vertices[f->vertexIds[0]], vertices[f->vertexIds[2]] - vertices[f->vertexIds[0]]));
	}

	int frameCount;
	rsmFile->read(reinterpret_cast<char*>(&frameCount), sizeof(int));
	frames.resize(frameCount);
	for (int i = 0; i < frameCount; i++)
	{
		frames[i] = new Frame();
		rsmFile->read(reinterpret_cast<char*>(&frames[i]->time), sizeof(int));

		float x, y, z, w;
		rsmFile->read(reinterpret_cast<char*>(&x), sizeof(float));
		rsmFile->read(reinterpret_cast<char*>(&y), sizeof(float));
		rsmFile->read(reinterpret_cast<char*>(&z), sizeof(float));
		rsmFile->read(reinterpret_cast<char*>(&w), sizeof(float));

		frames[i]->quaternion = glm::quat(w, x, y, z);
	}
}

void Rsm::Mesh::fetchChildren(std::map<std::string, Mesh* > meshes)
{
	for (std::map<std::string, Mesh*, std::less<std::string> >::iterator it = meshes.begin(); it != meshes.end(); it++)
	{
		if (it->second->parentName == name && it->second != this)
		{
			it->second->parent = this;
			children.push_back(it->second);
		}
	}
	for (unsigned int i = 0; i < children.size(); i++)
		children[i]->fetchChildren(meshes);
}

void Rsm::Mesh::calcMatrix1(int time)
{
	matrix1 = glm::mat4(1.0f);

	if (parent == NULL)
	{
		if (children.size() > 0)
			matrix1 = glm::translate(matrix1, glm::vec3(-model->bbrange.x, -model->bbmax.y, -model->bbrange.z));
		else
			matrix1 = glm::translate(matrix1, glm::vec3(0, -model->bbmax.y + model->bbrange.y, 0));
	}
	else
		matrix1 = glm::translate(matrix1, pos);

	if (frames.size() == 0)
	{
		if (fabs(rotangle) > 0.01)
			matrix1 = glm::rotate(matrix1, glm::radians(rotangle * 180.0f / 3.14159f), rotaxis); //TODO: double conversion
	}
	else
	{
		if (frames[frames.size() - 1]->time != 0)
		{
			int tick = time % frames[frames.size() - 1]->time;
			int current = 0;
			for (unsigned int i = 0; i < frames.size(); i++)
			{
				if (frames[i]->time > tick)
				{
					current = i - 1;
					break;
				}
			}
			if (current < 0)
				current = 0;

			int next = current + 1;
			if (next >= (int)frames.size())
				next = 0;

			float interval = ((float)(tick - frames[current]->time)) / ((float)(frames[next]->time - frames[current]->time));
#if 1
			glm::quat quat = glm::mix(frames[current]->quaternion, frames[next]->quaternion, interval);
#else
			bEngine::math::cQuaternion quat(
				(1 - interval) * animationFrames[current].quat.x + interval * animationFrames[next].quat.x,
				(1 - interval) * animationFrames[current].quat.y + interval * animationFrames[next].quat.y,
				(1 - interval) * animationFrames[current].quat.z + interval * animationFrames[next].quat.z,
				(1 - interval) * animationFrames[current].quat.w + interval * animationFrames[next].quat.w);
#endif

			quat = glm::normalize(quat);

			matrix1 = matrix1 * glm::toMat4(quat);

		}
		else
		{
			matrix1 *= glm::toMat4(glm::normalize(frames[0]->quaternion));
		}
	}

	matrix1 = glm::scale(matrix1, scale);
	//	if(nAnimationFrames == 0)
	//		cache1 = true;

	for (unsigned int i = 0; i < children.size(); i++)
		children[i]->calcMatrix1(time);
}

void Rsm::Mesh::calcMatrix2()
{
	matrix2 = glm::mat4(1.0f);

	if (parent == NULL && children.size() == 0)
		matrix2 = glm::translate(matrix2, -1.0f * model->bbrange);

	if (parent != NULL || children.size() != 0)
		matrix2 = glm::translate(matrix2, pos_);

	matrix2 *= offset;

	for (unsigned int i = 0; i < children.size(); i++)
		children[i]->calcMatrix2();
}


void Rsm::Mesh::setBoundingBox(glm::vec3& _bbmin, glm::vec3& _bbmax)
{
	int c;
	bbmin = glm::vec3(9999999, 9999999, 9999999);
	bbmax = glm::vec3(-9999999, -9999999, -9999999);

	if (parent != NULL)
		bbmin = bbmax = glm::vec3(0, 0, 0);

	glm::mat4 myMat = offset;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		for (int ii = 0; ii < 3; ii++)
		{
			glm::vec4 v = glm::vec4(vertices[faces[i]->vertexIds[ii]], 1);
			v = myMat * v;
			if (parent != NULL || children.size() != 0)
				v += glm::vec4(pos + pos_, 1);
			for (c = 0; c < 3; c++)
			{
				bbmin[c] = glm::min(bbmin[c], v[c]);
				bbmax[c] = glm::max(bbmax[c], v[c]);
			}
		}
	}
	bbrange = (bbmin + bbmax) / 2.0f;

	for (int c = 0; c < 3; c++)
		for (unsigned int i = 0; i < 3; i++)
		{
			_bbmax[c] = glm::max(_bbmax[c], bbmax[c]);
			_bbmin[c] = glm::min(_bbmin[c], bbmin[c]);
		}

	for (unsigned int i = 0; i < children.size(); i++)
		dynamic_cast<Mesh*>(children[i])->setBoundingBox(_bbmin, _bbmax);
}

void Rsm::Mesh::setBoundingBox2(glm::mat4& mat, glm::vec3& bbmin_, glm::vec3& bbmax_)
{
	glm::mat4 mat1 = mat * matrix1;
	glm::mat4 mat2 = mat * matrix1 * matrix2;
	for (unsigned int i = 0; i < faces.size(); i++)
	{
		for (int ii = 0; ii < 3; ii++)
		{
			glm::vec4 v = mat2 * glm::vec4(vertices[faces[i]->vertexIds[ii]], 1);
			bbmin_.x = glm::min(bbmin_.x, v.x);
			bbmin_.y = glm::min(bbmin_.y, v.y);
			bbmin_.z = glm::min(bbmin_.z, v.z);

			bbmax_.x = glm::max(bbmax_.x, v.x);
			bbmax_.y = glm::max(bbmax_.y, v.y);
			bbmax_.z = glm::max(bbmax_.z, v.z);
		}
	}

	for (unsigned int i = 0; i < children.size(); i++)
		dynamic_cast<Mesh*>(children[i])->setBoundingBox2(mat1, bbmin_, bbmax_);
}