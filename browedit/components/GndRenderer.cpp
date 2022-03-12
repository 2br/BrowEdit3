#include "GndRenderer.h"
#include "Gnd.h"
#include "Rsw.h"
#include <browedit/util/ResourceManager.h>
#include <browedit/Node.h>
#include <browedit/shaders/GndShader.h>
#include <browedit/gl/Texture.h>
#include <stb/stb_image_write.h>
#include <map>
#include <iostream>




GndRenderer::GndRenderer()
{
	renderContext = GndRenderContext::getInstance();

	gndShadow = new gl::Texture(shadowmapSize, shadowmapSize);
	gndShadowDirty = true;
}

void GndRenderer::render()
{
	if (!this->gnd)
	{ //todo: move a tryLoadGnd method
		this->gnd = node->getComponent<Gnd>();
		if (gnd)
		{
			chunks.resize((int)ceil(gnd->height / (float)CHUNKSIZE), std::vector<Chunk*>((int)ceil(gnd->width / (float)CHUNKSIZE), NULL));
			for (size_t y = 0; y < chunks.size(); y++)
				for (size_t x = 0; x < chunks[y].size(); x++)
					chunks[y][x] = new Chunk((int)(x * CHUNKSIZE), (int)(y * CHUNKSIZE), gnd, this);
		}
	}
	if (!this->gnd)
		return;
	if (!this->rsw)
		this->rsw = node->getComponent<Rsw>();
	if (!this->rsw)
		return;

	if (gnd->textures.size() != textures.size())
	{ //TODO: check if the texture names match
		std::size_t first = textures.size();
		for (std::size_t i = first; i < gnd->textures.size(); i++)
			textures.push_back(util::ResourceManager<gl::Texture>::load("data\\texture\\" + gnd->textures[first+i]->file));
	}

	if (gndShadowDirty)
	{
		char* data = new char[shadowmapSize * shadowmapSize * 4];
		int x = 0; int y = 0;
		for (size_t i = 0; i < gnd->lightmaps.size(); i++)
		{
			Gnd::Lightmap* lightMap = gnd->lightmaps[i];
			for (int xx = 0; xx < 8; xx++)
			{
				for (int yy = 0; yy < 8; yy++)
				{
					int xxx = 8 * x + xx;
					int yyy = 8 * y + yy;
					if (smoothColors)
					{
						data[4 * (xxx + shadowmapSize * yyy) + 0] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 0] >> 4) << 4;
						data[4 * (xxx + shadowmapSize * yyy) + 1] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 1] >> 4) << 4;
						data[4 * (xxx + shadowmapSize * yyy) + 2] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 2] >> 4) << 4;
						data[4 * (xxx + shadowmapSize * yyy) + 3] = lightMap->data[xx + 8 * yy];
					}
					else
					{
						data[4 * (xxx + shadowmapSize * yyy) + 0] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 0]);
						data[4 * (xxx + shadowmapSize * yyy) + 1] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 1]);
						data[4 * (xxx + shadowmapSize * yyy) + 2] = (lightMap->data[64 + 3 * (xx + 8 * yy) + 2]);
						data[4 * (xxx + shadowmapSize * yyy) + 3] = lightMap->data[xx + 8 * yy];
					}
				}
			}
			x++;
			if (x * 8 >= shadowmapSize)
			{
				x = 0;
				y++;
				if (y * 8 >= shadowmapSize)
				{
					std::cerr<< "Lightmap too big!" << std::endl;
					y = 0;
				}
			}
		}

		gndShadow->setSubImage(data, 0, 0, shadowmapSize, shadowmapSize);
		delete[] data;
		gndShadowDirty = false;
	}


	glActiveTexture(GL_TEXTURE1);
	gndShadow->bind();
	glActiveTexture(GL_TEXTURE0);
	auto shader = dynamic_cast<GndRenderContext*>(renderContext)->shader;
	shader->setUniform(GndShader::Uniforms::ModelViewMatrix, dynamic_cast<GndRenderContext*>(renderContext)->viewMatrix);


	glm::vec3 lightDirection;
	lightDirection[0] = -glm::cos(glm::radians((float)rsw->light.longitude)) * glm::sin(glm::radians((float)rsw->light.latitude));
	lightDirection[1] = glm::cos(glm::radians((float)rsw->light.latitude));
	lightDirection[2] = glm::sin(glm::radians((float)rsw->light.longitude)) * glm::sin(glm::radians((float)rsw->light.latitude));
	shader->setUniform(GndShader::Uniforms::lightAmbient, rsw->light.ambient);
	shader->setUniform(GndShader::Uniforms::lightDiffuse, rsw->light.diffuse);
	shader->setUniform(GndShader::Uniforms::lightDirection, lightDirection);
	shader->setUniform(GndShader::Uniforms::lightIntensity, rsw->light.intensity);

	shader->setUniform(GndShader::Uniforms::shadowMapToggle, viewLightmapShadow? 0.0f : 1.0f);
	shader->setUniform(GndShader::Uniforms::lightColorToggle, viewLightmapColor ? 1.0f : 0.0f);
	shader->setUniform(GndShader::Uniforms::lightToggle, viewLighting ? 0.0f : 1.0f);
	shader->setUniform(GndShader::Uniforms::colorToggle, viewColors ? 0.0f : 1.0f);
	shader->setUniform(GndShader::Uniforms::viewTextures, viewTextures ? 1.0f : 0.0f);

	for (auto r : chunks)
		for (auto c : r)
			c->render();

	
}


GndRenderer::GndRenderContext::GndRenderContext() : shader(util::ResourceManager<gl::Shader>::load<GndShader>())
{
	shader->use();
	shader->setUniform(GndShader::Uniforms::s_texture, 0);
	shader->setUniform(GndShader::Uniforms::s_lighting, 1);
}


void GndRenderer::GndRenderContext::preFrame(const glm::mat4& projectionMatrix, const glm::mat4& viewMatrix)
{
	shader->use();
	shader->setUniform(GndShader::Uniforms::ProjectionMatrix, projectionMatrix);
	this->viewMatrix = viewMatrix;
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3);
	glEnableVertexAttribArray(4); //TODO: vao
}



GndRenderer::Chunk::Chunk(int x, int y, Gnd* gnd, GndRenderer* renderer)
{
	this->dirty = true;
	this->rebuilding = false;
	this->x = x;
	this->y = y;
	this->gnd = gnd;
	this->renderer = renderer;
}


void GndRenderer::Chunk::render()
{
	if ((dirty || vbo.size() == 0) && !rebuilding)
		rebuild();

	if (vbo.size() > 0)
	{
		auto shader = dynamic_cast<GndRenderContext*>(renderer->renderContext)->shader;

		//TODO: vao
		vbo.bind();
		for (VboIndex& it : vertIndices)
		{
			if(it.texture != -1)
				renderer->textures[it.texture]->bind();
			else
			{
				shader->setUniform(GndShader::Uniforms::shadowMapToggle, 1.0f);
				shader->setUniform(GndShader::Uniforms::lightColorToggle, 0.0f);
				shader->setUniform(GndShader::Uniforms::colorToggle, 1.0f);
				shader->setUniform(GndShader::Uniforms::viewTextures, 0.0f);
			}
			//VertexP3T2T2C4N3
			glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(VertexP3T2T2C4N3), (void*)(0 * sizeof(float)));
			glVertexAttribPointer(1, 2, GL_FLOAT, false, sizeof(VertexP3T2T2C4N3), (void*)(3 * sizeof(float)));
			glVertexAttribPointer(2, 2, GL_FLOAT, false, sizeof(VertexP3T2T2C4N3), (void*)(5 * sizeof(float)));
			glVertexAttribPointer(3, 4, GL_FLOAT, false, sizeof(VertexP3T2T2C4N3), (void*)(7 * sizeof(float)));
			glVertexAttribPointer(4, 3, GL_FLOAT, false, sizeof(VertexP3T2T2C4N3), (void*)(11 * sizeof(float)));
			glDrawArrays(GL_TRIANGLES, (int)it.begin, (int)it.count);
			if (it.texture == -1)
			{
				shader->setUniform(GndShader::Uniforms::shadowMapToggle, renderer->viewLightmapShadow ? 0.0f : 1.0f);
				shader->setUniform(GndShader::Uniforms::lightColorToggle, renderer->viewLightmapColor ? 1.0f : 0.0f);
				shader->setUniform(GndShader::Uniforms::colorToggle, renderer->viewColors ? 0.0f : 1.0f);
				shader->setUniform(GndShader::Uniforms::viewTextures, renderer->viewTextures ? 1.0f : 0.0f);
			}
		}
		vbo.unBind();
	}
}


void GndRenderer::Chunk::rebuild()
{
	std::vector<VertexP3T2T2C4N3> vertices;
	std::map<int, std::vector<VertexP3T2T2C4N3> > verts;
	vertIndices.clear();

	for (int x = this->x; x < glm::min(this->x + CHUNKSIZE, (int)gnd->cubes.size()); x++)
	{
		for (int y = this->y; y < glm::min(this->y + CHUNKSIZE, (int)gnd->cubes[x].size()); y++)
		{
			Gnd::Cube* cube = gnd->cubes[x][y];

			if (cube->tileUp != -1)
			{
				Gnd::Tile* tile = gnd->tiles[cube->tileUp];
				assert(tile->lightmapIndex >= 0);

				glm::vec2 lm1((tile->lightmapIndex % shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize, (tile->lightmapIndex / shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize);
				glm::vec2 lm2(lm1 + glm::vec2(6.0f / shadowmapSize, 6.0f / shadowmapSize));

				glm::vec4 c1(1.0);
				if(y < gnd->height-1 && gnd->cubes[x][y + 1]->tileUp != -1)
					c1 = glm::vec4(gnd->tiles[gnd->cubes[x][y + 1]->tileUp]->color) / 255.0f;
				glm::vec4 c2(1.0f);
				if (x < gnd->width - 1 && y < gnd->height - 1 && gnd->cubes[x + 1][y + 1]->tileUp != -1)
					c2 = glm::vec4(gnd->tiles[gnd->cubes[x + 1][y + 1]->tileUp]->color) / 255.0f;
				glm::vec4 c3 = glm::vec4(tile->color) / 255.0f;
				glm::vec4 c4(1.0f);
				if (x < gnd->width - 1 && gnd->cubes[x + 1][y]->tileUp != -1)
					c4 = glm::vec4(gnd->tiles[gnd->cubes[x+1][y]->tileUp]->color) / 255.0f;

				VertexP3T2T2C4N3 v1(glm::vec3(10 * x, -cube->h3, 10 * gnd->height - 10 * y),			tile->v3, glm::vec2(lm1.x, lm2.y), c1,		cube->normals[2]);
				VertexP3T2T2C4N3 v2(glm::vec3(10 * x + 10, -cube->h4, 10 * gnd->height - 10 * y),		tile->v4, glm::vec2(lm2.x, lm2.y), c2,		cube->normals[3]);
				VertexP3T2T2C4N3 v3(glm::vec3(10 * x, -cube->h1, 10 * gnd->height - 10 * y + 10),		tile->v1, glm::vec2(lm1.x, lm1.y), c3,		cube->normals[0]);
				VertexP3T2T2C4N3 v4(glm::vec3(10 * x + 10, -cube->h2, 10 * gnd->height - 10 * y + 10),	tile->v2, glm::vec2(lm2.x, lm1.y), c4,		cube->normals[1]);

				verts[tile->textureIndex].push_back(v4); verts[tile->textureIndex].push_back(v2); verts[tile->textureIndex].push_back(v1);
				verts[tile->textureIndex].push_back(v4); verts[tile->textureIndex].push_back(v1); verts[tile->textureIndex].push_back(v3);
			}
			else if (renderer->viewEmptyTiles)
			{
				VertexP3T2T2C4N3 v1(glm::vec3(10 * x, -cube->h3, 10 * gnd->height - 10 * y),			glm::vec2(0), glm::vec2(0), glm::vec4(1.0f),	cube->normals[2]);
				VertexP3T2T2C4N3 v2(glm::vec3(10 * x + 10, -cube->h4, 10 * gnd->height - 10 * y),		glm::vec2(0), glm::vec2(0), glm::vec4(1.0f),	cube->normals[3]);
				VertexP3T2T2C4N3 v3(glm::vec3(10 * x, -cube->h1, 10 * gnd->height - 10 * y + 10),		glm::vec2(0), glm::vec2(0), glm::vec4(1.0f),	cube->normals[0]);
				VertexP3T2T2C4N3 v4(glm::vec3(10 * x + 10, -cube->h2, 10 * gnd->height - 10 * y + 10),	glm::vec2(0), glm::vec2(0), glm::vec4(1.0f),	cube->normals[1]);

				verts[-1].push_back(v3); verts[-1].push_back(v2); verts[-1].push_back(v1);
				verts[-1].push_back(v4); verts[-1].push_back(v2); verts[-1].push_back(v3);
			}
			if (cube->tileSide != -1 && x < gnd->width - 1)
			{
				Gnd::Tile* tile = gnd->tiles[cube->tileSide];
				assert(tile->lightmapIndex >= 0);

				glm::vec2 lm1((tile->lightmapIndex % shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize, (tile->lightmapIndex / shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize);
				glm::vec2 lm2(lm1 + glm::vec2(6.0f / shadowmapSize, 6.0f / shadowmapSize));


				glm::vec4 c1(1.0f);
				if (x < gnd->width - 1 && gnd->cubes[x + 1][y]->tileUp != -1)
					c1 = glm::vec4(gnd->tiles[gnd->cubes[x + 1][y]->tileUp]->color) / 255.0f;
				glm::vec4 c2(1.0);
				if (x < gnd->width - 1 && y < gnd->height - 1 && gnd->cubes[x + 1][y + 1]->tileUp != -1)
					c2 = glm::vec4(gnd->tiles[gnd->cubes[x + 1][y + 1]->tileUp]->color) / 255.0f;


				//up front
				VertexP3T2T2C4N3 v1(glm::vec3(10 * x + 10, -cube->h2, 10 * gnd->height - 10 * y + 10),					tile->v2, glm::vec2(lm2.x, lm1.y), c1, glm::vec3(1, 0, 0));
				//up back
				VertexP3T2T2C4N3 v2(glm::vec3(10 * x + 10, -cube->h4, 10 * gnd->height - 10 * y),						tile->v1, glm::vec2(lm1.x, lm1.y), c2, glm::vec3(1, 0, 0));
				//down front
				VertexP3T2T2C4N3 v3(glm::vec3(10 * x + 10, -gnd->cubes[x + 1][y]->h1, 10 * gnd->height - 10 * y + 10),	tile->v4, glm::vec2(lm2.x, lm2.y), c1, glm::vec3(1, 0, 0));
				//down back
				VertexP3T2T2C4N3 v4(glm::vec3(10 * x + 10, -gnd->cubes[x + 1][y]->h3, 10 * gnd->height - 10 * y),		tile->v3, glm::vec2(lm1.x, lm2.y), c2, glm::vec3(1, 0, 0));

				verts[tile->textureIndex].push_back(v3); verts[tile->textureIndex].push_back(v2); verts[tile->textureIndex].push_back(v1);
				verts[tile->textureIndex].push_back(v4); verts[tile->textureIndex].push_back(v2); verts[tile->textureIndex].push_back(v3);
			}
			if (cube->tileFront != -1 && y < gnd->height - 1)
			{
				Gnd::Tile* tile = gnd->tiles[cube->tileFront];
				assert(tile->lightmapIndex >= 0);

				glm::vec2 lm1((tile->lightmapIndex % shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize, (tile->lightmapIndex / shadowmapRowCount) * (8.0f / shadowmapSize) + 1.0f / shadowmapSize);
				glm::vec2 lm2(lm1 + glm::vec2(6.0f / shadowmapSize, 6.0f / shadowmapSize));

				glm::vec4 c1(1.0f);
				if (y < gnd->height - 1 && gnd->cubes[x][y + 1]->tileUp != -1)
					c1 = glm::vec4(gnd->tiles[gnd->cubes[x][y + 1]->tileUp]->color) / 255.0f;
				glm::vec4 c2(1.0);
				if (x < gnd->width - 1 && y < gnd->height - 1 && gnd->cubes[x + 1][y + 1]->tileUp != -1)
					c2 = glm::vec4(gnd->tiles[gnd->cubes[x + 1][y + 1]->tileUp]->color) / 255.0f;

				VertexP3T2T2C4N3 v1(glm::vec3(10 * x, -cube->h3, 10 * gnd->height - 10 * y),						tile->v1, glm::vec2(lm1.x, lm1.y), c1, glm::vec3(0, 0, 1));
				VertexP3T2T2C4N3 v2(glm::vec3(10 * x + 10, -cube->h4, 10 * gnd->height - 10 * y),					tile->v2, glm::vec2(lm2.x, lm1.y), c2, glm::vec3(0, 0, 1));
				VertexP3T2T2C4N3 v4(glm::vec3(10 * x + 10, -gnd->cubes[x][y + 1]->h2, 10 * gnd->height - 10 * y),	tile->v4, glm::vec2(lm2.x, lm2.y), c2, glm::vec3(0, 0, 1));
				VertexP3T2T2C4N3 v3(glm::vec3(10 * x, -gnd->cubes[x][y + 1]->h1, 10 * gnd->height - 10 * y),		tile->v3, glm::vec2(lm1.x, lm2.y), c1, glm::vec3(0, 0, 1));

				verts[tile->textureIndex].push_back(v3); verts[tile->textureIndex].push_back(v2); verts[tile->textureIndex].push_back(v1);
				verts[tile->textureIndex].push_back(v4); verts[tile->textureIndex].push_back(v2); verts[tile->textureIndex].push_back(v3);
			}
		}
	}

	for (auto it : verts)
	{
		vertIndices.push_back(VboIndex(it.first, vertices.size(), it.second.size()));
		vertices.insert(vertices.end(), it.second.begin(), it.second.end());
	}

	vbo.setData(vertices, GL_STATIC_DRAW);
	dirty = false;
	rebuilding = false;
}

void GndRenderer::setChunkDirty(int x, int y)
{
	chunks[y / CHUNKSIZE][x / CHUNKSIZE]->dirty = true;
}

void GndRenderer::setChunksDirty()
{
	for (auto y = 0; y < chunks.size(); y++)
		for (auto x = 0; x < chunks[y].size(); x++)
			chunks[y][x]->dirty = true;
}