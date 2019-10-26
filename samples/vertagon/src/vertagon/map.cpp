#include "vertagon/map.hpp"
#include "vertagon/game.hpp"
#include "vertagon/player.hpp"

#include <Wasabi/Renderers/DeferredRenderer/WGBufferRenderStage.hpp>
#include <Wasabi/Renderers/DeferredRenderer/WSceneCompositionRenderStage.hpp>
#include <Wasabi/Physics/Bullet/WBulletRigidBody.hpp>

class SkyVS : public WShader {
public:
    SkyVS(class Wasabi* const app) : WShader(app) {}

    virtual void Load(bool bSaveData = false) {
        m_desc.type = W_VERTEX_SHADER;
        m_desc.bound_resources = {W_BOUND_RESOURCE(
            W_TYPE_UBO, 0, "uboPerObject", {
                W_SHADER_VARIABLE_INFO(W_TYPE_MAT4X4, "wvp"),
            }
        )};
        m_desc.input_layouts = { W_INPUT_LAYOUT({
            W_SHADER_VARIABLE_INFO(W_TYPE_VEC_3), // position
            W_SHADER_VARIABLE_INFO(W_TYPE_VEC_3), // tangent
            W_SHADER_VARIABLE_INFO(W_TYPE_VEC_3), // normal
            W_SHADER_VARIABLE_INFO(W_TYPE_VEC_2), // UV
            W_SHADER_VARIABLE_INFO(W_TYPE_UINT, 1), // texture index
        }) };
        vector<uint8_t> code{
            #include "shaders/sky.vert.glsl.spv"
        };
        LoadCodeSPIRV((char*)code.data(), (int)code.size(), bSaveData);
    }
};

class SkyPS : public WShader {
public:
    SkyPS(class Wasabi* const app) : WShader(app) {}

    virtual void Load(bool bSaveData = false) {
        m_desc.type = W_FRAGMENT_SHADER;
        m_desc.bound_resources = {};
        vector<uint8_t> code{
            #include "shaders/sky.frag.glsl.spv"
        };
        LoadCodeSPIRV((char*)code.data(), (int)code.size(), bSaveData);
    }
};

Map::Map(Wasabi* app): m_app(app) {
    m_sky = nullptr;
    m_skyGeometry = nullptr;
    m_skyEffect = nullptr;

    m_firstTowerUpdate = -1.0f;

    m_towerTexture = nullptr;
    m_towerParams.numPlatforms = 10; // number of platforms to create
    m_towerParams.platformLength = 120.0f; // the length of the 2D (top-down view) arc representing each platform
    m_towerParams.distanceBetweenPlatforms = 10.0f; // distance (arc) to leave between platforms
    m_towerParams.platformHeight = 2.0f; // height from the beginning of the platform to the end
    m_towerParams.heightBetweenPlatforms = 15.0f; // height difference between the end of a platform and beginning of the next one
    m_towerParams.towerRadius = 90.0f; // radius of the tower
    m_towerParams.platformWidth = 40.0f;
    m_towerParams.platformResWidth = 5;
    m_towerParams.platformResLength = 20;
    m_towerParams.xzRandomness = 3.0f;
    m_towerParams.yRandomness = 1.0f;
    m_towerParams.lengthRandomness = 60.0f;
}

WError Map::Load() {
    Cleanup();

    WGBufferRenderStage* GBufferRenderStage = (WGBufferRenderStage*)m_app->Renderer->GetRenderStage("WGBufferRenderStage");
    WSceneCompositionRenderStage* SceneCompositionRenderStage = (WSceneCompositionRenderStage*)m_app->Renderer->GetRenderStage("WSceneCompositionRenderStage");

    /**
     * Liughting
     */
    m_app->LightManager->GetDefaultLight()->Hide();
    SceneCompositionRenderStage->SetAmbientLight(WColor(0.3f, 0.3f, 0.3f));

    /**
     * Sky
     */
    m_skyGeometry = new WGeometry(m_app);
    WError status = m_skyGeometry->CreateSphere(-5000.0f, 28, 28);
    if (!status) return status;
    status = ((Vertagon*)m_app)->UnsmoothFeometryNormals(m_skyGeometry);
    if (!status) return status;

    SkyPS* skyPS = new SkyPS(m_app);
    skyPS->Load();
    SkyVS* skyVS = new SkyVS(m_app);
    skyVS->Load();

    if (!skyPS->Valid() || !skyVS->Valid())
        status = WError(W_NOTVALID);

    if (status) {
        m_skyEffect = new WEffect(m_app);
        m_skyEffect->SetRenderFlags(EFFECT_RENDER_FLAG_RENDER_FORWARD | EFFECT_RENDER_FLAG_TRANSLUCENT);

        status = m_skyEffect->BindShader(skyVS);
        if (status) {
            status = m_skyEffect->BindShader(skyPS);
            if (status) {
                status = m_skyEffect->BuildPipeline(SceneCompositionRenderStage->GetRenderTarget());
            }
        }
    }

    W_SAFE_REMOVEREF(skyPS);
    W_SAFE_REMOVEREF(skyVS);
    if (!status) return status;

    m_sky = m_app->ObjectManager->CreateObject(m_skyEffect, 0);
    if (!m_sky) return WError(W_ERRORUNK);
    m_sky->ClearEffects();
    m_sky->AddEffect(m_skyEffect, 0);
    status = m_sky->SetGeometry(m_skyGeometry);
    if (!status) return status;
    m_sky->SetName("Mapsky");

    m_sky->GetMaterials().SetVariable("color", WColor(0.9f, 0.2f, 0.2f));
    m_sky->GetMaterials().SetVariable("isTextured", 0);

    status = BuildTower();
    if (!status) return status;

    return status;
}

void Map::Update(float fDeltaTime) {
    WCamera* cam = m_app->CameraManager->GetDefaultCamera();
    m_sky->GetMaterials().SetVariable("wvp", WTranslationMatrix(((Vertagon*)m_app)->m_player->GetPosition()) * cam->GetViewMatrix() * cam->GetProjectionMatrix());

    /**
     * Update the platforms
     */
    float time = m_app->Timer.GetElapsedTime() / 6.0f;
    if (m_firstTowerUpdate < 0.0f)
        m_firstTowerUpdate = time;
    time -= m_firstTowerUpdate;
    for (uint32_t i = 0; i < m_tower.size(); i++) {
        ComputePlatformCurrentCenter(i, time);
        m_tower[i].rigidBody->SetPosition(m_tower[i].curCenter);
    }
}

void Map::Cleanup() {
    W_SAFE_REMOVEREF(m_sky);
    W_SAFE_REMOVEREF(m_skyGeometry);
    W_SAFE_REMOVEREF(m_skyEffect);

    W_SAFE_REMOVEREF(m_towerTexture);
    for (auto platform : m_tower) {
        W_SAFE_REMOVEREF(platform.geometry);
        W_SAFE_REMOVEREF(platform.rigidBody);
        W_SAFE_REMOVEREF(platform.object);
    }
}

void Map::ComputePlatformCurrentCenter(uint32_t i, float time) {
    float phase = (float)i * 5.0f;
    float scale = 1.0f+ (float)i / (float)m_tower.size();
    float magnitude = m_towerParams.heightBetweenPlatforms * 0.4f;
    m_tower[i].curCenter = m_tower[i].center + WVector3(0.0f, std::sin(phase + time * scale) * magnitude, 0.0f);
}

WError Map::BuildTower() {
    WError status = WError(W_SUCCEEDED);

    uint8_t pixels[2*2*4] = {
        76, 187, 27, 255,
        76, 187, 27, 255,
        155, 118, 83, 255,
        155, 118, 83, 255,
    };
    m_towerTexture = m_app->ImageManager->CreateImage(static_cast<void*>(pixels), 2, 2, VK_FORMAT_R8G8B8A8_UNORM);
    if (!m_towerTexture)
        return WError(W_ERRORUNK);

    float curAngle = 0.0f;
    float curHeight = 0.0f;
    float anglePerGap = W_RADTODEG(m_towerParams.distanceBetweenPlatforms / m_towerParams.towerRadius); // angle occupied the gap between platforms
    for (uint32_t platform = 0; platform < m_towerParams.numPlatforms && status; platform++) {
        float platformLength = m_towerParams.platformLength - m_towerParams.lengthRandomness / 2.0f + (m_towerParams.lengthRandomness * (std::rand() % 10000) / 10000.0f);
        float platformAngle = W_RADTODEG(platformLength / m_towerParams.towerRadius); // angle occupied by each platform
        status = BuildTowerPlatform(curAngle, curAngle + platformAngle, curHeight, curHeight + m_towerParams.platformHeight);
        curAngle += platformAngle + anglePerGap;
        curHeight += m_towerParams.platformHeight + m_towerParams.heightBetweenPlatforms;
    }

    return status;
}

WError Map::BuildTowerPlatform(float angleFrom, float angleTo, float heightFrom, float heightTo) {
    WError status = WError(W_SUCCEEDED);
    TOWER_PLATFORM p;
    float midAngle = (angleTo + angleFrom) / 2.0f;
    p.center =
        (WVector3(cosf(W_DEGTORAD(midAngle)), 0.0f, sinf(W_DEGTORAD(midAngle))) * m_towerParams.towerRadius) +
        WVector3(0.0f, (heightTo + heightFrom) / 2.0f, 0.0f);

    p.object = m_app->ObjectManager->CreateObject();
    p.geometry = new WGeometry(m_app);
    p.rigidBody = new WBulletRigidBody(m_app);

    if (!p.object) status = WError(W_ERRORUNK);

    if (status) {
        status = BuildPlatformGeometry(p.geometry, p.center, angleFrom, angleTo, heightFrom, heightTo);
        if (status) {
            status = p.object->SetGeometry(p.geometry);
            if (status) {
                p.object->SetName("Platform-" + std::to_string(heightFrom) + "-" + std::to_string(heightTo));

                p.object->GetMaterials().SetTexture("diffuseTexture", m_towerTexture);

                status = p.rigidBody->Create(W_RIGID_BODY_CREATE_INFO::ForComplexGeometry(p.geometry, true, nullptr, p.center));
                if (status) {
                    p.rigidBody->BindObject(p.object, p.object);
                }
            }
        }
    }

    if (!status) {
        W_SAFE_REMOVEREF(p.geometry);
        W_SAFE_REMOVEREF(p.object);
        W_SAFE_REMOVEREF(p.rigidBody);
    } else {
        m_tower.push_back(p);
        ComputePlatformCurrentCenter(m_tower.size() - 1, 0.0f);
    }

    return status;
}

WError Map::BuildPlatformGeometry(WGeometry* geometry, WVector3 center, float angleFrom, float angleTo, float heightFrom, float heightTo) {
    uint32_t xsegs = m_towerParams.platformResWidth;
    uint32_t zsegs = m_towerParams.platformResLength;
    float width = m_towerParams.platformWidth;

    /**
     * Create a grid of triangles that is curved (using the radius of the tower)
     * with two parts: a top part and a bottom part. The top part is the platform
     * to walk on and the bottom part is to make it floating-island-like.
     * Multiply by 2 (one for top part one for bottom).
     */
    uint32_t numVertices = ((xsegs + 1) * (zsegs + 1) * 2 * 3) *2;
    uint32_t numIndices = numVertices;

    //allocate the plain vertices
    vector<WDefaultVertex> vertices(numVertices);
    vector<uint32_t> indices(vertices.size());

    std::vector<WVector3> randomValues((xsegs+2) * (zsegs+2));
    for (uint32_t i = 0; i < randomValues.size(); i++)
        randomValues[i] =
            WVector3(-m_towerParams.xzRandomness/2.0f, -m_towerParams.yRandomness/2.0f, -m_towerParams.xzRandomness/2.0f) +
            WVector3(m_towerParams.xzRandomness, m_towerParams.yRandomness, m_towerParams.xzRandomness) *
            WVector3(rand() % 10000, rand() % 10000, rand() % 10000) / 10000.0f;

    auto computeNormal = [&vertices](uint32_t v1, uint32_t v2, uint32_t v3) {
        WVector3 p1 = vertices[v1].pos;
        WVector3 p2 = vertices[v2].pos;
        WVector3 p3 = vertices[v3].pos;
        WVector3 U = p2 - p1;
        WVector3 V = p3 - p1;
        WVector3 norm = WVec3Normalize(WVector3(U.y * V.z - U.z * V.y, U.z * V.x - U.x * V.z, U.x * V.y - U.y * V.x));
        vertices[v1].norm = vertices[v2].norm = vertices[v3].norm = norm;
    };

    auto randomness = [&randomValues](uint32_t l, uint32_t w, uint32_t W) {
        return randomValues[w * W + l];
    };

    /**
     * Create the top part: a curved grid
     */
    uint32_t curVert = 0;
    for (uint32_t celll = 0; celll < zsegs + 1; celll++) {
        float _v1 = ((float)celll / (float)(zsegs+1));
        float _v2 = ((float)(celll+1) / (float)(zsegs+1));
        for (uint32_t cellw = 0; cellw < xsegs + 1; cellw++) {
            float _u1 = (float)(cellw) / (float)(xsegs+1);
            float _u2 = (float)(cellw+1) / (float)(xsegs+1);

            auto vtx = [this, &center, &angleFrom, &angleTo, &heightFrom, &heightTo, &width](float u, float v, WVector3 randomness) {
                auto xc = [this, &angleFrom, &angleTo, &width](float u, float v) {
                    return cosf(W_DEGTORAD(v * (angleTo - angleFrom) + angleFrom)) * (m_towerParams.towerRadius - width / 2.0f + width * u);
                };
                auto yc = [&heightFrom, &heightTo, &width](float u, float v) {
                    float h = heightFrom + (heightTo - heightFrom) * v;
                    if (std::abs(u) < W_EPSILON || std::abs(u - 1.0f) < W_EPSILON || std::abs(v) < W_EPSILON || std::abs(v - 1.0f) < W_EPSILON)
                        h -= width / 10.0f;
                    return h;
                };
                auto zc = [this, &angleFrom, &angleTo, &width](float u, float v) {
                    return sinf(W_DEGTORAD(v * (angleTo - angleFrom) + angleFrom)) * (m_towerParams.towerRadius - width / 2.0f + width * u);
                };

                WDefaultVertex vtx = WDefaultVertex(xc(u, v), yc(u, v), zc(u, v), 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, u, v);
                vtx.pos += randomness - center;
                return vtx;
            };

            vertices[curVert + 0] = vtx(_u1, _v1, randomness(celll+0, cellw+0, xsegs+1));
            vertices[curVert + 1] = vtx(_u1, _v2, randomness(celll+1, cellw+0, xsegs+1));
            vertices[curVert + 2] = vtx(_u2, _v2, randomness(celll+1, cellw+1, xsegs+1));

            vertices[curVert + 3] = vtx(_u1, _v1, randomness(celll+0, cellw+0, xsegs+1));
            vertices[curVert + 4] = vtx(_u2, _v2, randomness(celll+1, cellw+1, xsegs+1));
            vertices[curVert + 5] = vtx(_u2, _v1, randomness(celll+0, cellw+1, xsegs+1));

            // Those 2 vertices are outliers (they make flat triangles that look bad on edges), hide their triangles
            if (cellw == xsegs && celll == 0)
                vertices[curVert + 5].pos = WVector3(vertices[curVert + 1].pos.x, vertices[curVert + 5].pos.y, vertices[curVert + 1].pos.z);
            else if (cellw == 0 && celll == zsegs)
                vertices[curVert + 1].pos = WVector3(vertices[curVert + 5].pos.x, vertices[curVert + 1].pos.y, vertices[curVert + 5].pos.z);

            for (uint32_t i = 0; i < 6; i++)
                indices[curVert + i] = curVert + i;
            computeNormal(curVert + 0, curVert + 1, curVert + 2);
            computeNormal(curVert + 3, curVert + 4, curVert + 5);
            curVert += 6;
        }
    }

    /**
     * Create the bottom part: mirror of the top part but with a different height
     * (and has flipped indices)
     */
    float heightRandomness = 5.0f + 100.0f * m_towerParams.yRandomness * (float)(std::rand() % 10000) / 10000.0f;
    for (; curVert < numVertices; curVert += 3) {
        for (uint32_t i = 0; i < 3; i++) {
            vertices[curVert + i] = vertices[curVert + i - numVertices / 2];
            indices[curVert + i] = curVert + 2 - i;

            // adjust the y component
            float u = vertices[curVert + i].texC.x;
            float v = vertices[curVert + i].texC.y;
            WVector2 dist = WVector2(0.5f - std::abs(u - 0.5f), 0.5f - std::abs(v - 0.5f)) * 2.0f;
            float distFromEdge = std::min(dist.x * ((float)m_towerParams.platformResWidth / (float)m_towerParams.platformResLength), dist.y); // between 0 (at edges) and 1 (in the center)
            vertices[curVert + i].pos.y -= sqrt(distFromEdge * width * heightRandomness);
            if (distFromEdge > W_EPSILON)
                vertices[curVert + i].pos.y -= width / 10.0f;
        }
        computeNormal(curVert + 2, curVert + 1, curVert + 0);
    }

    /**
     * Fix the UVs
     */
    for (uint32_t i = 0; i < numVertices; i++) {
        vertices[i].texC = WVector2((i < numVertices / 2) ? 0.1f : 0.9f, (i < numVertices / 2) ? 0.1f : 0.9f);
    }

    return geometry->CreateFromDefaultVerticesData(vertices, indices);
}

WVector3 Map::GetSpawnPoint() const {
    return m_tower[0].center + WVector3(0.0f, 5.0f, 0.0f);
}

void Map::RandomSpawn(WOrientation* object) const {
    WVector3 spawn = m_tower[rand() % m_tower.size()].curCenter + WVector3(0.0f, 5.0f, 0.0f);
    object->SetPosition(spawn);
}

float Map::GetMinPoint() const {
    return -200.0f;
}
