#include <particle.hh>
#include <shaderprogram.hh>
#include <texture.hh>
#include <glm/gtc/matrix_transform.hpp>

Particle::Particle(const glm::vec2& position, float size, const glm::vec4& color, float life,
                   const glm::vec2& velocity, const glm::vec2& acceleration):
    position(position),
    size(size),
    color(color),
    life(life),
    maxLife(life),
    velocity(velocity),
    acceleration(acceleration)
{}

void Particle::update(float dt)
{
    position = position + velocity * dt + 0.5f * acceleration * glm::pow(dt, 2.f);
    velocity = velocity + acceleration * dt;
}

ParticleGenerator::ParticleGenerator(float spawnTime, const glm::vec2& position, float life, const glm::vec4 spawnRange,
                                     bool isSpawnCirlce, GLenum sFactor, GLenum dFactor, const PData& pData,
                                     std::function<void(float)> customUpdate, std::size_t numParticles):
    spawnTime(spawnTime),
    position(position),
    accumulator(0.f),
    life(life),
    spawnRange(spawnRange),
    sFactor(sFactor),
    dFactor(dFactor),
    lastActive(-1),
    maxParticles(static_cast<std::size_t>(1.f/spawnTime * pData.lifeRange.y + 1.f)),
    pData(pData),
    isSpawnCircle(isSpawnCirlce),
    customUpdate(customUpdate)
{
    std::random_device rd;
    rng.seed(rd());

    if(numParticles > maxParticles)
        maxParticles = numParticles;

    pPool.reserve(maxParticles);
    vboData.reserve(maxParticles);

    vao.bind();

    GLfloat vertices[] =
    {
        // pos....texCoord
        0.f, 0.f, 0.f, 0.f,
        1.f, 0.f, 1.f, 0.f,
        1.f, 1.f, 1.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
        0.f, 1.f, 0.f, 1.f,
        0.f, 0.f, 0.f, 0.f
    };

    vboStatic.bind(GL_ARRAY_BUFFER);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    vboDynamic.bind(GL_ARRAY_BUFFER);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(maxParticles * sizeof(Instance)),
                 nullptr, GL_DYNAMIC_DRAW);

    // color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), 0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);

    // model matrix
    //    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
    //                          reinterpret_cast<const void*>(sizeof(glm::vec4)));
    //    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
    //                          reinterpret_cast<const void*>(2 * sizeof(glm::vec4)));
    //    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
    //                          reinterpret_cast<const void*>(3 * sizeof(glm::vec4)));
    //    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(Instance),
    //                          reinterpret_cast<const void*>(4 * sizeof(glm::vec4)));

    //    glEnableVertexAttribArray(2);
    //    glEnableVertexAttribArray(3);
    //    glEnableVertexAttribArray(4);
    //    glEnableVertexAttribArray(5);
    //    glVertexAttribDivisor(2, 1);
    //    glVertexAttribDivisor(3, 1);
    //    glVertexAttribDivisor(4, 1);
    //    glVertexAttribDivisor(5, 1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), reinterpret_cast<const void*>(sizeof(glm::vec4)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
}

std::size_t ParticleGenerator::spawnParticle()
{
    // size
    std::uniform_real_distribution<float> uni(pData.sizeRange.x, pData.sizeRange.y);
    float pSize = uni(rng);
    // position
    uni = std::uniform_real_distribution<float>(0.f, 1.f);
    glm::vec2 pPosition;
    pPosition.x = uni(rng) * spawnRange.z;
    if(isSpawnCircle)
    {
        assert(int(spawnRange.z) == int(spawnRange.w));
        float yRange = glm::sqrt(glm::pow(spawnRange.z / 2.f, 2.f) - glm::pow(pPosition.x - spawnRange.z / 2.f, 2.f));
        uni = std::uniform_real_distribution<float>(-yRange, yRange);
        pPosition.y = uni(rng) + spawnRange.z / 2.f;
    }
    else
        pPosition.y = uni(rng) * spawnRange.w;

    pPosition.x += position.x + spawnRange.x - pSize / 2.f;
    pPosition.y += position.y + spawnRange.y - pSize / 2.f;
    // color
    glm::vec4 pColor;
    uni = std::uniform_real_distribution<float>(pData.lowColor.r, pData.highColor.r);
    pColor.r = uni(rng);
    uni = std::uniform_real_distribution<float>(pData.lowColor.g, pData.highColor.g);
    pColor.g = uni(rng);
    uni = std::uniform_real_distribution<float>(pData.lowColor.b, pData.highColor.b);
    pColor.b = uni(rng);
    uni = std::uniform_real_distribution<float>(pData.lowColor.a, pData.highColor.a);
    pColor.a = uni(rng);
    // life
    uni = std::uniform_real_distribution<float>(pData.lifeRange.x, pData.lifeRange.y);
    float pLife = uni(rng);
    // velocity
    glm::vec2 pVelocity;
    uni = std::uniform_real_distribution<float>(pData.velocityRange.x, pData.velocityRange.z);
    pVelocity.x = uni(rng);
    uni = std::uniform_real_distribution<float>(pData.velocityRange.y, pData.velocityRange.w);
    pVelocity.y = uni(rng);
    // acceleration
    glm::vec2 pAcceleration;
    uni = std::uniform_real_distribution<float>(pData.accelerationRange.x, pData.accelerationRange.z);
    pAcceleration.x = uni(rng);
    uni = std::uniform_real_distribution<float>(pData.accelerationRange.y, pData.accelerationRange.w);
    pAcceleration.y = uni(rng);

    if(static_cast<std::size_t>(lastActive + 1) == pPool.size())
    {
        assert(static_cast<std::size_t>(lastActive + 1 + 1) <= maxParticles);
        pPool.emplace_back(pPosition, pSize, pColor, pLife, pVelocity, pAcceleration);
        vboData.emplace_back();
    }
    else
    {
        pPool[static_cast<std::size_t>(lastActive + 1)] = {pPosition, pSize, pColor, pLife, pVelocity, pAcceleration};
    }
    ++lastActive;

    return static_cast<std::size_t>(lastActive);
}

void ParticleGenerator::killParticle(Particle& particle)
{
    std::swap(particle, pPool[static_cast<std::size_t>(lastActive)]);
    --lastActive;
}

void ParticleGenerator::updateVboInstance(std::size_t i)
{
    vboData[i].color.r = pPool[i].color.r / 255.f;
    vboData[i].color.g = pPool[i].color.g / 255.f;
    vboData[i].color.b = pPool[i].color.b / 255.f;
    vboData[i].color.a = pPool[i].color.a;

    vboData[i].model = glm::vec3(pPool[i].position, pPool[i].size);

    //    glm::mat4 model;
    //    model = glm::translate(model, glm::vec3(pPool[i].position, 0.f));
    //    model = glm::scale(model, glm::vec3(pPool[i].size, pPool[i].size, 1.f));

    //    vboData[i].model = std::move(model);
}

void ParticleGenerator::update(float dt)
{
    for(std::size_t i = 0; i < static_cast<std::size_t>(lastActive + 1); ++i)
    {
        pPool[i].life -= dt;
        if(pPool[i].life <= 0.f)
            killParticle(pPool[i]);
        else
        {
            pPool[i].update(dt);
            if(pData.customUpdate)
                pData.customUpdate(dt, pPool[i]);
            updateVboInstance(i);
        }
    }

    if(customUpdate)
        customUpdate(dt);
    accumulator += dt;
    while(accumulator >= spawnTime)
    {
        updateVboInstance(spawnParticle());
        accumulator -= spawnTime;
    }
    if(life > 0.f)
        life -= dt;
}

void ParticleGenerator::render(const ShaderProgram& shader)
{
    vboDynamic.bind(GL_ARRAY_BUFFER);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(static_cast<std::size_t>(lastActive + 1) * sizeof(Instance)),
                    vboData.data());

    shader.bind();
    pData.texture->bind();
    vao.bind();

    glBlendFunc(sFactor, dFactor);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, lastActive + 1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

bool ParticleGenerator::isActive() const
{
    return life <= -1000 || life > 0;
}

const glm::vec2& ParticleGenerator::getPosition() const
{
    return position;
}

void ParticleGenerator::setPosition(const glm::vec2& position)
{
    this->position = position;
}

const PData& ParticleGenerator::getPdata() const
{
    return pData;
}

PData& ParticleGenerator::getPdata()
{
    return const_cast<PData&>(
                static_cast<const ParticleGenerator&>(*this).getPdata()
                );
}
