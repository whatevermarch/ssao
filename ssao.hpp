#ifndef SSAO_HPP
#define SSAO_HPP

#include <chrono>

#include <QOpenGLShaderProgram>
#include <QVector3D>

#include "cgbase/cgopenglwidget.hpp"

class SSAO : public Cg::OpenGLWidget
{
private:
    //  OpenGL core 3.3 func. wrapper object
    //QOpenGLFunctions_3_3_Core _ogl33Func;

    std::chrono::time_point<std::chrono::system_clock> _lastTimePoint;

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
        _prg_ssao,
        _prg_ssao_blur,
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
            albedo,
            shadow;
    } 
    _gBuffer;
    unsigned int _fbo_geom;

    //  objects for ssao buffer
    unsigned int _tex_ssao, _tex_ssao_blur,
        _fbo_ssao, _fbo_ssao_blur,
        _tex_noise;

    //  ssao kernel and noise texture object
    QVector<QVector3D> _ssaoKernel, _ssaoNoise;


    //  intialize scene objects
    void initializeScene();

    //  setup SSAO pipeline, kernel and noise texture
    void setupSSAOPass();

    //  setup SSAO kernel and noise texture
    void setupSSAOKernel();

    //  setup shadow map pipeline
    void setupShadowPass();


public:
    SSAO();
    ~SSAO();

    void initializeGL() override;
    void paintGL(const QMatrix4x4& P, const QMatrix4x4& V, int w, int h) override;
    void keyPressEvent(QKeyEvent* event) override;
};

#endif
