/*
 Implicit skinning
 Copyright (C) 2013 Rodolphe Vaillant, Loic Barthe, Florian Cannezin,
 Gael Guennebaud, Marie Paule Cani, Damien Rohmer, Brian Wyvill,
 Olivier Gourmel

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License 3 as published by
 the Free Software Foundation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>
 */
#ifndef BONE_HPP__
#define BONE_HPP__

#include <cassert>
#include "cuda_compiler_interop.hpp"
#include "point_cu.hpp"
#include "bbox.hpp"
#include "bone_type.hpp"

// TODO : to be moved in Skeleton_env
struct Skeleton;

/**
  @class Bone_cu
  @brief Mother class of various bones types

  A bone defines a segment between two 3D points.
  Attribute '_org' defines the starting point of the segment.

  The second ending point can be computed with the direction of the bone _dir
  whose magnitude equals the lenght of the bone.

  Bones points toward the skeleton leaves. Usualy bones at the leaves are zero
  length

  @code                       ->
       _org           _dir   (_org + _dir)
         +-------------->---------+
      joint                    son_joint (if any)
  @endcode

  Usually bones are associated to an implicit primitive. Thus this class is
  often subclass.
*/
class Bone_cu {
public:
    IF_CUDA_DEVICE_HOST
    Bone_cu() { }

    IF_CUDA_DEVICE_HOST
    Bone_cu(const Point_cu& p1, const Point_cu& p2, float r):
        _org(p1),
        _radius(r),
        _dir(p2-p1),
        _length((p2-p1).norm())
    { }

    IF_CUDA_DEVICE_HOST
    Bone_cu(const Point_cu& org, const Vec3_cu& dir, float length, float r):
        _org(org),
        _radius(r),
        _dir( dir.normalized() * length),
        _length( length )
    { }

    // -------------------------------------------------------------------------
    /// @name Getters
    // -------------------------------------------------------------------------
    IF_CUDA_DEVICE_HOST Point_cu  org()    const{ return _org;        }
    IF_CUDA_DEVICE_HOST Point_cu  end()    const{ return _org + _dir; }
    IF_CUDA_DEVICE_HOST float     length() const{ return _length;     }
    IF_CUDA_DEVICE_HOST float     radius() const{ return _radius;     }
    IF_CUDA_DEVICE_HOST Vec3_cu   dir()    const{ return _dir;        }

    IF_CUDA_DEVICE_HOST void set_length (float  l ){ _length = l;    }
    IF_CUDA_DEVICE_HOST void set_radius (float  r ){ _radius = r;    }
    IF_CUDA_DEVICE_HOST void incr_radius(float ext){ _radius += ext; }

    // -------------------------------------------------------------------------
    /// @name Setters
    // -------------------------------------------------------------------------

    IF_CUDA_DEVICE_HOST
    void set_start_end(const Point_cu& p0, const Point_cu& p1){
        _org = p0;
        _dir = p1 - p0;
        _length = _dir.norm();
    }

    IF_CUDA_DEVICE_HOST
    void set_orientation(const Point_cu& org, const Vec3_cu& dir){
        _org = org;
        _dir = dir.normalized() * _length;
    }

    // -------------------------------------------------------------------------
    /// @name Utilities
    // -------------------------------------------------------------------------

    /// 'p' is projected on the bone,
    /// then it returns the distance from the  origine '_org'
    IF_CUDA_DEVICE_HOST
    float dist_proj_to(const Point_cu& p) const
    {
        const Vec3_cu op = p - _org;
        return op.dot(_dir.normalized());
    }

    /// Orthogonal distance from the bone line to a point
    IF_CUDA_DEVICE_HOST
    float dist_ortho_to(const Point_cu& p) const
    {
        const Vec3_cu op = p - _org;
        return op.cross(_dir.normalized()).norm();
    }

    /// squared distance from a point p to the bone's segment.
    IF_CUDA_DEVICE_HOST
    float dist_sq_to(const Point_cu& p) const {
        Vec3_cu op = p - _org;
        float x = op.dot(_dir) / (_length * _length);
        x = fminf(1.f, fmaxf(0.f, x));
        Point_cu proj = _org + _dir * x;
        float d = proj.distance_squared(p);
        return d;
    }

    /// euclidean distance from a point p to the bone's segment.
    IF_CUDA_DEVICE_HOST
    float dist_to(const Point_cu& p) const {
        return sqrtf( dist_sq_to( p ) );
    }

    /// project p on the bone segment if the projection is outside the segment
    /// then returns the origine or the end point of the bone.
    IF_CUDA_DEVICE_HOST
    Point_cu project(const Point_cu& p) const
    {
        const Vec3_cu op = p - _org;
        float d = op.dot(_dir.normalized()); // projected dist from origin

        if(d < 0)            return _org;
        else if(d > _length) return _org + _dir;
        else                 return _org + _dir.normalized() * d;
    }

    /// Get the local frame of the bone. This method only guarantes to generate
    /// a frame with an x direction parallel to the bone and centered about '_org'
    IF_CUDA_DEVICE_HOST
    Transfo get_frame() const
    {
        Vec3_cu x = _dir.normalized();
        Vec3_cu ortho = x.cross(Vec3_cu(0.f, 1.f, 0.f));
        Vec3_cu z, y;
        if (ortho.norm_squared() < 1e-06f * 1e-06f)
        {
            ortho = Vec3_cu(0.f, 0.f, 1.f).cross(x);
            y = ortho.normalized();
            z = x.cross(y).normalized();
        }
        else
        {
            z = ortho.normalized();
            y = z.cross(x).normalized();
        }

        return Transfo(Mat3_cu(x, y, z), _org.to_vector() );
    }

    // -------------------------------------------------------------------------
    /// @name Attributes
    // -------------------------------------------------------------------------
// TODO: must be private
    Point_cu _org; ///< Bone origin (first joint position)
    float _radius; ///< Bone radius
    Vec3_cu _dir;  ///< Bone direction towards its son if any
    float _length; ///< Bone length (o + v.normalized*length = bone_end_point)
};
// =============================================================================

/** @class Bone_cu
  @brief Subclass of Bone_cu.

  @see Bone_type Bone_cu
*/
class Bone : public Bone_cu {
public:
    friend struct Skeleton;
    /// A bone identifier
    typedef int Id;

    Bone() : Bone_cu(), _bone_id(-1) {
        _length = 0.f;
        _radius = 0.f;
        _dir    = Vec3_cu(0.f, 0.f, 0.f);
        _org    = Point_cu(0.f, 0.f, 0.f);
    }

    virtual ~Bone(){}

    Id get_bone_id() const { return _bone_id; }

    Bone_cu get_bone_cu() const {
        assert(_bone_id != -1  );
        assert(_length  >= 0.f );
        assert(_radius  >= 0.f );
        return Bone_cu(_org, _org+_dir, _radius);
    }

    /// Get the bone type
    /// @see Bone_type
    virtual EBone::Bone_t get_type() const = 0;

    /// Get the oriented bounding box associated to the bone
    virtual OBBox_cu get_obbox() const;
    /// Get the axis aligned bounding box associated to the bone
    virtual BBox_cu get_bbox() const;

protected:
    Id _bone_id; ///< Bone identifier in skeleton class
};

// =============================================================================

#include "cylinder.hpp"

/** @class Bone_cylinder
  @brief Subclass of a Bone.
*/
class Bone_cylinder : public Bone {
public:
    Bone_cylinder() : Bone() { }

    ~Bone_cylinder(){  }

    /// Implicit cylinder is constructed from the bone data. Changing the bone
    /// properties change the cylinder properties. (thereby no setter is needed)
    Cylinder get_cylinder() const { return Cylinder(_org, _org+_dir, _radius); }

    EBone::Bone_t get_type() const { return EBone::CYLINDER; }

    OBBox_cu get_obbox() const;

    BBox_cu get_bbox() const;
};
// =============================================================================

#include "hermiteRBF.hpp"

/** @class Bone_hrbf
  @brief Subclass of a Bone. Adds an hrbf primitive attribute
*/
class Bone_hrbf : public Bone {
public:
    /// @param rad radius used to convert hrbf from global to compact support
    Bone_hrbf(float rad) : Bone() {
        _hrbf.initialize();
        _hrbf.set_radius(rad);
    }

    ~Bone_hrbf(){
        _hrbf.clear();
    }

    HermiteRBF& get_hrbf(){ return _hrbf; }

    const HermiteRBF& get_hrbf() const { return _hrbf; }

    EBone::Bone_t get_type() const { return EBone::HRBF; }

    OBBox_cu get_obbox() const;

    BBox_cu get_bbox() const;

    void set_hrbf_radius(float rad){ _hrbf.set_radius(rad); }

    float get_hrbf_radius() const { return _hrbf.get_radius(); }

private:
    HermiteRBF _hrbf;
};
// =============================================================================

/** @class Bone_ssd
  @brief No implicit primitive are bound to this type of bone
*/
class Bone_ssd : public Bone{
public:
    Bone_ssd() : Bone(){ }

    ~Bone_ssd(){  }

    EBone::Bone_t get_type() const { return EBone::SSD; }
};
// =============================================================================

#include "precomputed_prim.hpp"

/**
    @class Bone_precomputed
    @brief Subclass of a Bone. Adds an precomputed implicit primitive attribute
*/
class Bone_precomputed : public Bone {
public:
    Bone_precomputed(const OBBox_cu& obbox) : Bone(), _obbox(obbox){
        _primitive.initialize();
    }

    ~Bone_precomputed(){
        _primitive.clear();
    }

    Precomputed_prim& get_primitive(){ return _primitive; }

    EBone::Bone_t get_type() const { return EBone::PRECOMPUTED; }

    OBBox_cu get_obbox() const;

    BBox_cu get_bbox() const;

private:
    Precomputed_prim _primitive;
    OBBox_cu         _obbox;
};
// =============================================================================

namespace EBone {

/// @param type Bone's type from the enum field of Bone_type namespace
std::string type_to_string(int type);

}

#endif // BONE_HPP__