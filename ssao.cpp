#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <random>

#include <QApplication>
#include <QSurfaceFormat>
#include <QKeyEvent>
#include <QtMath>
#include <QOpenGLFunctions_3_3_Core>

#include "cgbase/cggeometries.hpp"
#include "cgbase/cgtools.hpp"

#include "ssao.hpp"

#define LIGHT_POS_DISTANCE 1.7f

#define SHADOW_MAP_WIDTH 1024

//  inline function to perform linear interpolation
inline float lerp(float a, float b, float f) { return a + f * (b - a); }

//  function to generate 2D texture with fix filtering params.
void createTexture(GLsizei width, GLsizei height,
    GLint inFormat, GLenum format, GLenum type,
    GLint filtering, GLint wrapping,
    const GLvoid* data, unsigned int& texture)
{
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, inFormat,
        width, height, 0, format, type, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);
    CG_ASSERT_GLCHECK();
}

//  function to build shader program from glsl code
//  inline bits indicate if this shader module is a file to be loaded or not
//  0x00000002 means the second least significant bit (fragment shader) is just
//  a string, no  need to perform I/O load
void createShaderProgram(QOpenGLShaderProgram& program, 
    const char* vs, const char* fs, int inlineBits)
{
    QString shaderCode;

    shaderCode.append(inlineBits & 0x00000001 ? vs : Cg::loadFile(vs));
    program.addShaderFromSourceCode(QOpenGLShader::Vertex,
        Cg::prependGLSLVersion(shaderCode));
    shaderCode.clear();

    shaderCode.append(inlineBits & 0x00000002 ? fs : Cg::loadFile(fs));
    program.addShaderFromSourceCode(QOpenGLShader::Fragment,
        Cg::prependGLSLVersion(shaderCode));
    shaderCode.clear();

    program.link();
    CG_ASSERT_GLCHECK();
}


SSAO::SSAO() :
    _kd(0.5f), _ks(0.5f), _shininess(30.0f),
    _lightAzimuthAngle(0)
{
    this->_lightDir = -QVector3D(1.0f, 1.0f, 0.0f).normalized();
}

SSAO::~SSAO()
{
}

//  intialize scene objects
void SSAO::initializeScene()
{
    // Set up buffer objects for the geometry
    QVector<float> positions, normals, texCoords;
    QVector<unsigned int> indices;

    //  setup a plane
    Cg::quad(positions, normals, texCoords, indices, 1);
    this->_vao_plane = Cg::createVertexArrayObject(positions, normals, texCoords, indices);
    this->_idxCount_plane = indices.size();
    CG_ASSERT_GLCHECK();
    this->_mmat_plane.scale(1.5f);
    this->_mmat_plane.rotate(-90.0f, 1.0f, 0.0f, 0.0f);

    //  setup a teapot
    //  it will be on top of the plane for sure
    Cg::loadObj(":teapot.obj", positions, normals, texCoords, indices);
    this->_vao_model = Cg::createVertexArrayObject(positions, normals, texCoords, indices);
    this->_idxCount_model = indices.size();
    CG_ASSERT_GLCHECK();
    this->_mmat_model.translate(0.0f, 0.5f, 0.0f);
}

//  setup SSAO pipeline, kernel and noise texture
void SSAO::setupSSAOPass()
{
    /////////////////////////////////////////
    //  Setup G-Buffer
    /////////////////////////////////////////
    // - position color buffer
    GLsizei screenWidth = this->width(), screenHeight = this->height();
    ::createTexture(screenWidth, screenHeight,
        GL_RGB16F, GL_RGB, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_gBuffer.position);

    // - normal color buffer
    ::createTexture(screenWidth, screenHeight,
        GL_RGB16F, GL_RGB, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_gBuffer.normal);

    // - color + specular color buffer
    ::createTexture(screenWidth, screenHeight,
        GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_gBuffer.albedo);

    // - shadow coord buffer
    ::createTexture(screenWidth, screenHeight,
        GL_RGB16F, GL_RGB, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_gBuffer.shadow);

    //  attach g-buffer as FBOs
    glGenFramebuffers(1, &this->_fbo_geom);
    glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_geom);
    unsigned int attachments[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[0], GL_TEXTURE_2D, this->_gBuffer.position, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[1], GL_TEXTURE_2D, this->_gBuffer.normal, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[2], GL_TEXTURE_2D, this->_gBuffer.albedo, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[3], GL_TEXTURE_2D, this->_gBuffer.shadow, 0);
    /*this->_ogl33Func.*/glDrawBuffers(4, attachments);
    //  also, attach depth render buffer to this fbo.
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, screenWidth, screenHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CG_ASSERT_GLCHECK();

    // Set up a pipeline for g-buffer pass
    ::createShaderProgram(this->_prg_geom, "vs_geom.glsl", "fs_geom.glsl", 0);

    /////////////////////////////////////////
    //  Setup SSAO
    /////////////////////////////////////////
    //  setup ssao color buffer
    ::createTexture(screenWidth, screenHeight,
        GL_RED, GL_RGB, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_tex_ssao);

    //  attach ssao buffer as FBO
    glGenFramebuffers(1, &this->_fbo_ssao);
    glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_ssao);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->_tex_ssao, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CG_ASSERT_GLCHECK();

    //  set up a pipeline for ssao
    ::createShaderProgram(this->_prg_ssao, 
        "vs_deferred.glsl",
        "fs_ssao.glsl",
        0);
    //  set sampler location for all input textures
    this->_prg_ssao.bind();
    this->_prg_ssao.setUniformValue("g_position", 0);
    this->_prg_ssao.setUniformValue("g_normal", 1);
    this->_prg_ssao.setUniformValue("noise_texture", 2);

    //  setup ssao blur buffer
    ::createTexture(screenWidth, screenHeight,
        GL_RED, GL_RGB, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_tex_ssao_blur);

    //  attach ssao buffer as FBO
    glGenFramebuffers(1, &this->_fbo_ssao_blur);
    glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_ssao_blur);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->_tex_ssao_blur, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CG_ASSERT_GLCHECK();

    //  set up a pipeline for ssao
    ::createShaderProgram(this->_prg_ssao_blur, 
        "vs_deferred.glsl",
        "fs_ssao_blur.glsl",
        0);
    //  set sampler location for all input textures
    this->_prg_ssao_blur.bind();
    this->_prg_ssao_blur.setUniformValue("ssao_texture", 0);

    //  setup SSAO kernel and noise texture
    this->setupSSAOKernel();
}

//  setup SSAO kernel and noise texture
void SSAO::setupSSAOKernel()
{
    // generate sample kernel
    // ----------------------
    std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;
    for (unsigned int i = 0; i < 64; ++i)
    {
        QVector3D sample(randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator));
        sample.normalize();
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0;

        // scale samples s.t. they're more aligned to center of kernel
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        this->_ssaoKernel.push_back(sample);
    }

    // generate noise texture
    // ----------------------
    for (unsigned int i = 0; i < 16; i++)
    {
        QVector3D noise(randomFloats(generator) * 2.0 - 1.0, 
            randomFloats(generator) * 2.0 - 1.0, 
            0.0f); // rotate around z-axis (in tangent space)
        this->_ssaoNoise.push_back(noise);
    }
    ::createTexture(4, 4, 
        GL_RGB32F, GL_RGB, GL_FLOAT, 
        GL_NEAREST, GL_REPEAT, 
        this->_ssaoNoise.data(), this->_tex_noise);
}

//  setup shadow map pipeline
void SSAO::setupShadowPass()
{
    //  setup depth map
    ::createTexture(SHADOW_MAP_WIDTH, SHADOW_MAP_WIDTH,
        GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT, GL_FLOAT,
        GL_NEAREST, GL_CLAMP_TO_EDGE,
        NULL, this->_tex_depth);

    // attach depth texture as FBO's depth buffer
    glGenFramebuffers(1, &this->_fbo_depth);
    glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_depth);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->_tex_depth, 0);
    /*this->_ogl33Func.*/glDrawBuffer(GL_NONE);
    /*this->_ogl33Func.*/glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    CG_ASSERT_GLCHECK();

    //  set up a pipeline for shadow map
    ::createShaderProgram(this->_prg_shadow, "                                  \
            layout(location = 0) in vec4 position;                             \
                                                                                \
            uniform mat4 mvp_matrix;                                            \
                                                                                \
            void main()                                                         \
            {                                                                   \
                gl_Position = mvp_matrix * position;                            \
            }                                                                   \
        ",
        "                                                                       \
            void main() {}                                                      \
        ",
        3);
}

void SSAO::initializeGL()
{
    Cg::OpenGLWidget::initializeGL();
    //this->_ogl33Func.initializeOpenGLFunctions();
    this->navigator()->initialize(QVector3D(0.0f, 0.0f, 0.0f), 1.4f);

    // Set up buffer objects for the geometry
    this->initializeScene();

    //  turn on to enable SSAO
    this->setupSSAOPass();

    //  turn on to enable shadow
    this->setupShadowPass();

    // Set up a pipeline for main scene
    //::createShaderProgram(this->_prg_main, "vs.glsl", "fs.glsl", 0);
    ::createShaderProgram(this->_prg_main, "vs_deferred.glsl", "fs_lighting.glsl", 0);
    //  set sampler location for all input textures
    this->_prg_main.bind();
    this->_prg_main.setUniformValue("g_position", 0);
    this->_prg_main.setUniformValue("g_normal", 1);
    this->_prg_main.setUniformValue("g_albedo", 2);
    this->_prg_main.setUniformValue("g_shadow", 3);
    this->_prg_main.setUniformValue("ssao_texture", 4);
    this->_prg_main.setUniformValue("shadow_map", 5);

    //  start recording time
    this->_lastTimePoint = std::chrono::system_clock::now();
}

void SSAO::paintGL(const QMatrix4x4& P, const QMatrix4x4& V, int w, int h)
{
    //  capture the start time
    auto right_now = std::chrono::system_clock::now();
    std::chrono::duration<double> diff_time = right_now - this->_lastTimePoint;
    this->_lastTimePoint = right_now;

    Cg::OpenGLWidget::paintGL(P, V, w, h);

    //  calculate light space PV. This is used in both passes
    QMatrix4x4 PV_shadow;
    float near_plane = 0.5f, far_plane = 7.5f;
    PV_shadow.ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
    PV_shadow.lookAt(-this->_lightDir * LIGHT_POS_DISTANCE, QVector3D(), QVector3D(0.0f, 1.0f, 0.0f));

    //  update the model's animation
    //  our angular velocity of the model is pi/2 rad/s
    this->_mmat_model.rotate(90.0f * static_cast<float>(diff_time.count()), 0.0f, 1.0f, 0.0f);
    
    //  Render Pass 1: Shadow
    {
        // Set up view
        glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_depth);
        glViewport(0, 0, SHADOW_MAP_WIDTH, SHADOW_MAP_WIDTH);
        glClear(GL_DEPTH_BUFFER_BIT);

        //  inverse face culling
        glCullFace(GL_FRONT);

        // Render: draw model
        this->_prg_shadow.bind();
        this->_prg_shadow.setUniformValue("mvp_matrix", PV_shadow * this->_mmat_model);
        glBindVertexArray(this->_vao_model);
        glDrawElements(GL_TRIANGLES, this->_idxCount_model, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  Render: draw plane
        this->_prg_shadow.setUniformValue("mvp_matrix", PV_shadow * this->_mmat_plane);
        glBindVertexArray(this->_vao_plane);
        glDrawElements(GL_TRIANGLES, this->_idxCount_plane, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  Set back to backface culling
        glCullFace(GL_BACK);

        //  retreat from this framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    //  set new viewport and enable depth testing
    glViewport(0, 0, w, h);
    glEnable(GL_DEPTH_TEST);

    //  Render Pass 2: G-Buffer
    {
        // Set up framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_geom);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        //  initialize model matrix and combined model-view matrix as well.
        QMatrix4x4 VM;

        // Render: draw model
        this->_prg_geom.bind();
        this->_prg_geom.setUniformValue("projection_matrix", P);
        VM = V * this->_mmat_model;
        this->_prg_geom.setUniformValue("modelview_matrix", VM);
        this->_prg_geom.setUniformValue("normal_matrix", VM.normalMatrix());
        this->_prg_geom.setUniformValue("mvp_matrix_shadow", PV_shadow * this->_mmat_model);
        this->_prg_geom.setUniformValue("surface_color", QVector3D(1.0f, 0.0f, 0.4f));
        glBindVertexArray(this->_vao_model);
        glDrawElements(GL_TRIANGLES, this->_idxCount_model, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  Render: draw plane
        VM = V * this->_mmat_plane;
        this->_prg_geom.setUniformValue("modelview_matrix", VM);
        this->_prg_geom.setUniformValue("normal_matrix", VM.normalMatrix());
        this->_prg_geom.setUniformValue("mvp_matrix_shadow", PV_shadow * this->_mmat_plane);
        this->_prg_geom.setUniformValue("surface_color", QVector3D(0.8f, 0.8f, 1.0f));
        glBindVertexArray(this->_vao_plane);
        glDrawElements(GL_TRIANGLES, this->_idxCount_plane, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  retreat from this framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    //  Render Pass 3: SSAO
    {
        // Set up framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_ssao);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render: draw ssao texture
        this->_prg_ssao.bind();
        // Send kernel + rotation 
        this->_prg_ssao.setUniformValueArray("sampling_points", this->_ssaoKernel.data(), 64);
        this->_prg_ssao.setUniformValue("projection_matrix", P);
        this->_prg_ssao.setUniformValue("noiseScale", QVector2D( w / 4.0f, h / 4.0f));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.position);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.normal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, this->_tex_noise);
        glBindVertexArray(this->_vao_plane);
        glDrawElements(GL_TRIANGLES, this->_idxCount_plane, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  retreat from this framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    //  Render Pass 4: Blurring
    {
        // Set up framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, this->_fbo_ssao_blur);
        glClear(GL_COLOR_BUFFER_BIT);

        //  Render: draw blurred ssao texture
        this->_prg_ssao_blur.bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->_tex_ssao);
        glBindVertexArray(this->_vao_plane);
        glDrawElements(GL_TRIANGLES, this->_idxCount_plane, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();

        //  retreat from this framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    
    //  Render Pass 5: Main Pass
    {
        // Set up view
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Render: lighting
        this->_prg_main.bind();
        this->_prg_main.setUniformValue("light_dir", this->_lightDir);
        this->_prg_main.setUniformValue("kd", _kd);
        this->_prg_main.setUniformValue("ks", _ks);
        this->_prg_main.setUniformValue("shininess", _shininess);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.position);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.normal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.albedo);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, this->_gBuffer.shadow);
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, this->_tex_ssao_blur);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, this->_tex_depth);
        glBindVertexArray(this->_vao_plane);
        glDrawElements(GL_TRIANGLES, this->_idxCount_plane, GL_UNSIGNED_INT, 0);
        CG_ASSERT_GLCHECK();
    }
}

void SSAO::keyPressEvent(QKeyEvent* event)
{
    float angleInRadian;
    switch(event->key()) {
    case Qt::Key_Escape:
        quit();
        break;
    case Qt::Key_D:
        if (event->modifiers() == Qt::ShiftModifier)
            _kd = std::min(_kd + 0.05f, 1.0f);
        else
            _kd = std::max(_kd - 0.05f, 0.0f);
        break;
    case Qt::Key_S:
        if (event->modifiers() == Qt::ShiftModifier)
            _ks = std::min(_ks + 0.05f, 1.0f);
        else
            _ks = std::max(_ks - 0.05f, 0.0f);
        break;
    case Qt::Key_P:
        if (event->modifiers() == Qt::ShiftModifier)
            _shininess = std::min(_shininess + 3.0f, 120.0f);
        else
            _shininess = std::max(_shininess - 3.0f, 1.0f);
        break;
    case Qt::Key_Left:
        //  x = rcos(-), z = rsin(-)
        this->_lightAzimuthAngle = (this->_lightAzimuthAngle - 2) % 360;
        angleInRadian = this->_lightAzimuthAngle * M_PI / 180.0f;
        this->_lightDir = -QVector3D(cosf(angleInRadian), 1.0f, sinf(angleInRadian)).normalized();
        break;
    case Qt::Key_Right:
        //  x = rcos(-), z = rsin(-)
        this->_lightAzimuthAngle = (this->_lightAzimuthAngle + 2) % 360;
        angleInRadian = this->_lightAzimuthAngle * M_PI / 180.0f;
        this->_lightDir = -QVector3D(cosf(angleInRadian), 1.0f, sinf(angleInRadian)).normalized();
        break;
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 5);
    QSurfaceFormat::setDefaultFormat(format);
    SSAO example;
    Cg::init(argc, argv, &example);
    return app.exec();
}
