#pragma once

#include "Utils.h"
#include "ShaderConstants.h"
#include "VoxelTexture.h"
#include "FullScreenQuad.h"

class MipMapGenerator
{
private:

    GLuint mipmapProgram;
    VoxelTexture* voxelTexture;
    FullScreenQuad* fullScreenQuad;
    GLuint uCurrentMip;

    uint indexConverter(uint sideLength, glm::uvec3 index3d)
    {
        return index3d.x + index3d.y*sideLength + index3d.z*sideLength*sideLength;
    }

public:

    void begin(VoxelTexture* voxelTexture, FullScreenQuad* fullScreenQuad)
    {
        this->voxelTexture = voxelTexture;
        this->fullScreenQuad = fullScreenQuad;

        // Create mipmap shader program
        std::string vertexShaderSource = SHADER_DIRECTORY + "fullscreenQuadInstanced.vert";
        std::string fragmentShaderSource = SHADER_DIRECTORY + "mipmap.frag";
        mipmapProgram = Utils::OpenGL::createShaderProgram(vertexShaderSource, fragmentShaderSource);
    }

    void generateMipMapGPU()
    {
        // Disable culling, depth test, rendering
        Utils::OpenGL::setRenderState(false, false, false);

        // Bind voxelTexture's color for writing
        glBindImageTexture(COLOR_IMAGE_3D_BINDING_BASE, voxelTexture->colorTexture, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA8);

        // Mip-map
        int voxelGridLength = voxelTexture->voxelGridLength;
        glUseProgram(mipmapProgram);
        for(uint i = 1; i < voxelTexture->numMipMapLevels; i++)
        {
            glBindImageTexture(COLOR_IMAGE_3D_BINDING_CURR, voxelTexture->colorTexture, i-1, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);
            glBindImageTexture(COLOR_IMAGE_3D_BINDING_NEXT, voxelTexture->colorTexture, i, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA8);

            // Call the program for each mip map level.
            int voxelGridLength = voxelTexture->mipMapInfoArray[i].gridLength;
            Utils::OpenGL::setViewport(voxelGridLength, voxelGridLength);
            fullScreenQuad->displayInstanced(voxelGridLength);
            glMemoryBarrier(GL_ALL_BARRIER_BITS);//GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
    }

    // This code is not super efficient since it is a short term solution that will be replaced by GPU-based mipmap generation
    void generateMipMapCPU()
    {
        int mipMapSideLength = voxelTexture->mipMapInfoArray[0].gridLength;
        for(uint i = 1; i < voxelTexture->numMipMapLevels; i++)
        {
            int prevMipMapSideLength = mipMapSideLength;
            mipMapSideLength = voxelTexture->mipMapInfoArray[i].gridLength;

            TextureData prevMipData;
            prevMipData.colorData.resize(prevMipMapSideLength*prevMipMapSideLength*prevMipMapSideLength);

            glActiveTexture(GL_TEXTURE0 + COLOR_TEXTURE_3D_BINDING);
            glBindTexture(GL_TEXTURE_3D, voxelTexture->colorTexture);
            glGetTexImage(GL_TEXTURE_3D, i-1, GL_RGBA, GL_UNSIGNED_BYTE, &prevMipData.colorData[0]);
            
            TextureData currMipData;
            currMipData.colorData.resize(mipMapSideLength*mipMapSideLength*mipMapSideLength);

            for(int j = 0; j < mipMapSideLength; j++)
            for(int k = 0; k < mipMapSideLength; k++)
            for(int l = 0; l < mipMapSideLength; l++)
            {
                glm::vec4 summedColor(0);

                glm::uvec3 index3d(j*2, k*2, l*2);  
                for(int m = 0; m < 2; m++)
                for(int n = 0; n < 2; n++)
                for(int o = 0; o < 2; o++)
                {
                    glm::uvec3 neighbor = index3d + glm::uvec3(m,n,o);
                    uint neighborIndex1d = indexConverter(prevMipMapSideLength, neighbor);
                   
                    glm::vec4 neighborColor = glm::vec4(prevMipData.colorData[neighborIndex1d])/255.0f;
                    summedColor += glm::vec4(glm::vec3(neighborColor)*neighborColor.a, neighborColor.a);
                }

                glm::vec3 averageColor = glm::vec3(summedColor)/summedColor.a;
                glm::vec4 finalColor(averageColor, summedColor.a/8);

                uint index1d = indexConverter(mipMapSideLength, glm::uvec3(j,k,l));
                currMipData.colorData[index1d] = glm::u8vec4(finalColor*255.0f);
            }

            voxelTexture->setData(currMipData, mipMapSideLength, i);
        }
    }
};