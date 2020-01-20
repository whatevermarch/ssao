#ifndef SSAO_HPP
#define SSAO_HPP

#include <QOpenGLShaderProgram>
#include <QVector3D>

#include "cgbase/cgopenglwidget.hpp"

class SSAO : public Cg::OpenGLWidget
{
private:
    //  OpenGL core 3.3 func. wrapper object
    QOpenGLFunctions_3_3_Core _ogl33Func;

    //  Lighting State
    float _kd, _ks, _shininess;

    //////////////////////////////////////
    //  start my work here
    //////////////////////////////////////

    //  scene properties
    //  light direction towards the center of the scene
    //  WARNING!! normalize it everytime
    QVector3D _lightDir;
    //  azimuth angle to move the direction vector
    int _lightAzimuthAngle;

    //  shared objects
    QOpenGLShaderProgram _prg_main, 
        _prg_geom, 
        _prg_shadow;

    //  objects for plane (base scene)
    unsigned int _vao_plane,
        _idxCount_plane;
    QMatrix4x4 _mmat_plane;

    //  objects for main model
    unsigned int _vao_model,
        _idxCount_model;
    QMatrix4x4 _mmat_model;

    //  objects for depth map
    unsigned int _tex_depth,
        _fbo_depth;

    //  objects for g-buffer
    struct GBuffer
    {
        unsigned int position, 
            normal, 
            albedo;
    } 
    _gBuffer;
    unsigned int _fbo_geom;

    //  objects for ssao buffer
    unsigned int _tex_ssao,
        _fbo_ssao;


    //  intialize scene objects
    void initializeScene();

    //  initialize intermediate buffers used during the full pipieline
    //  e.g. g-buffer, shadow map, etc.
    void initializeIntermediateBuffers();

    //  initialize all shader programs
    void initializeShaders();

    //  initialize all required framebuffers
    void initializeFramebuffers();

public:
    SSAO();
    ~SSAO();

    void initializeGL() override;
    void paintGL(const QMatrix4x4& P, const QMatrix4x4& V, int w, int h) override;
    void keyPressEvent(QKeyEvent* event) override;
};

#endif
