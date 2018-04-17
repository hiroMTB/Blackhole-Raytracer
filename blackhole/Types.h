#pragma once

#include "glm/glm.hpp"
#include "vector"
#include <OpenCL/opencl.h>
#include <cmath>

#define PHYSICS_G 6.67e-11
#define PHYSICS_C 3e8

class CScene
{
public:
    CScene(){}
};

class Blackhole
{
public:
    glm::dvec3 m_position;
    double m_mass;
    double m_radius;
    double m_accreation_disk_coef;
    Blackhole(glm::dvec3 position, double mass, double accreation_disk_coef) : m_position(position), m_mass(mass), m_accreation_disk_coef(accreation_disk_coef)
    {
        m_radius = 2 * mass * PHYSICS_G / pow(PHYSICS_C, 2);
    }
};


class AccreationDisk
{
    double m_radius;
    AccreationDisk(double radius) : m_radius(radius){};
};

struct SRay
{
  glm::dvec3 m_start;
  glm::dvec3 m_dir;
};

struct SCamera
{
  glm::dvec3 m_pos;          // Camera position and orientation
  glm::dvec3 m_forward;      // Orthonormal basis
  glm::dvec3 m_right;
  glm::dvec3 m_up;

  glm::dvec2 m_viewAngle;    // View angles, rad
  glm::dvec2 m_resolution;  // Image resolution: w, h

  std::vector<glm::vec3> m_pixels;  // Pixel array
};

struct SMesh
{
  std::vector<glm::vec3> m_vertices;  // vertex positions
  std::vector<glm::uvec3> m_triangles;  // vetrex indices
};

class Settings
{
public:
    int xRes;
    int yRes;
    float xViewAngle;
    float cameraX;
    float cameraY;
    float cameraZ;
    float cameraDistance;
    float blackholeMass;
    float blackholeRadius;
    float diskCoef;
    Settings(int _xRes,
             int _yRes,
             float _xViewAngle,
             float _cameraX,
             float _cameraY,
             float _cameraZ,
             float _blackholeMass,
             float _diskCoef)
    : xRes(_xRes),
    yRes(_yRes),
    xViewAngle(_xViewAngle),
    cameraX(_cameraX),
    cameraY(_cameraY),
    cameraZ(_cameraZ),
    blackholeMass(_blackholeMass),
    diskCoef(_diskCoef) {
        blackholeRadius = 2 * blackholeMass * PHYSICS_G / pow(PHYSICS_C, 2);
        cameraDistance = sqrt(pow(cameraX, 2) + pow(cameraY, 2) + pow(cameraZ, 2));
    }
};


typedef struct
{
    int start;
    cl_float3 m_start;
    cl_float3 m_dir;
} CLRay;

typedef struct
{
    cl_float3 m_pos;          // Camera position and orientation
    cl_float3 m_forward;      // Orthonormal basis
    cl_float3 m_right;
    cl_float3 m_up;
    
    cl_float2 m_viewAngle;    // View angles, rad
    cl_float2 m_resolution;  // Image resolution: w, h
    
}CLCamera;

typedef struct
{
    cl_float cameraDistance;
    cl_float blackholeMass;
    cl_float blackholeRadius;
    cl_float diskCoef;
} CLSettings;

typedef struct
{
    cl_float3 intersect1;
    cl_float3 intersect2;
    cl_float3 intersect3;
    int type;
    // 1 - disk, blackhole
    // 2 - blackhole
    // 3 - disk, background
    // 4 - background
    //
} Result;
