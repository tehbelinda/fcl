/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2011-2014, Willow Garage, Inc.
 *  Copyright (c) 2014-2016, Open Source Robotics Foundation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Open Source Robotics Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/** \author Jia Pan */

#include "fcl/BVH/BV_fitter.h"
#include "fcl/BVH/BVH_utility.h"
#include <limits>

namespace fcl
{


static const double kIOS_RATIO = 1.5;
static const double invSinA = 2;
static const double invCosA = 2.0 / sqrt(3.0);
// static const double sinA = 0.5;
static const double cosA = sqrt(3.0) / 2.0;

static inline void axisFromEigen(const Matrix3d& eigenV, const Vector3d& eigenS, Matrix3d& axis)
{
  int min, mid, max;

  if(eigenS[0] > eigenS[1])
  {
    max = 0;
    min = 1;
  }
  else
  {
    min = 0;
    max = 1;
  }

  if(eigenS[2] < eigenS[min])
  {
    mid = min;
    min = 2;
  }
  else if(eigenS[2] > eigenS[max])
  {
    mid = max;
    max = 2;
  }
  else
  {
    mid = 2;
  }

  axis.col(0) = eigenV.row(max);
  axis.col(1) = eigenV.row(mid);
  axis.col(2) = axis.col(0).cross(axis.col(1));
}

namespace OBB_fit_functions
{

void fit1(Vector3d* ps, OBB& bv)
{
  bv.To = ps[0];
  bv.axis.setIdentity();
  bv.extent.setConstant(0);
}

void fit2(Vector3d* ps, OBB& bv)
{
  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  Vector3d p1p2 = p1 - p2;
  FCL_REAL len_p1p2 = p1p2.norm();
  p1p2.normalize();

  bv.axis.col(0) = p1p2;
  generateCoordinateSystem(bv.axis);

  bv.extent << len_p1p2 * 0.5, 0, 0;
  bv.To = 0.5 * (p1 + p2);
}

void fit3(Vector3d* ps, OBB& bv)
{
  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  const Vector3d& p3 = ps[2];
  Vector3d e[3];
  e[0] = p1 - p2;
  e[1] = p2 - p3;
  e[2] = p3 - p1;
  FCL_REAL len[3];
  len[0] = e[0].squaredNorm();
  len[1] = e[1].squaredNorm();
  len[2] = e[2].squaredNorm();

  int imax = 0;
  if(len[1] > len[0]) imax = 1;
  if(len[2] > len[imax]) imax = 2;

  bv.axis.col(2) = e[0].cross(e[1]);
  bv.axis.col(2).normalize();
  bv.axis.col(0) = e[imax];
  bv.axis.col(0).normalize();
  bv.axis.col(1) = bv.axis.col(2).cross(bv.axis.col(0));

  getExtentAndCenter(ps, NULL, NULL, NULL, 3, bv.axis, bv.To, bv.extent);
}

void fit6(Vector3d* ps, OBB& bv)
{
  OBB bv1, bv2;
  fit3(ps, bv1);
  fit3(ps + 3, bv2);
  bv = bv1 + bv2;
}


void fitn(Vector3d* ps, int n, OBB& bv)
{
  Matrix3d M;
  Matrix3d E;
  Vector3d s = Vector3d::Zero(); // three eigen values

  getCovariance(ps, NULL, NULL, NULL, n, M);
  eigen(M, s, E);
  axisFromEigen(E, s, bv.axis);

  // set obb centers and extensions
  getExtentAndCenter(ps, NULL, NULL, NULL, n, bv.axis, bv.To, bv.extent);
}

}


namespace RSS_fit_functions
{
void fit1(Vector3d* ps, RSS& bv)
{
  bv.Tr = ps[0];
  bv.axis.setIdentity();
  bv.l[0] = 0;
  bv.l[1] = 0;
  bv.r = 0;
}

void fit2(Vector3d* ps, RSS& bv)
{
  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  Vector3d p1p2 = p1 - p2;
  FCL_REAL len_p1p2 = p1p2.norm();
  p1p2.normalize();

  bv.axis.col(0) = p1p2;
  generateCoordinateSystem(bv.axis);
  bv.l[0] = len_p1p2;
  bv.l[1] = 0;

  bv.Tr = p2;
  bv.r = 0;
}

void fit3(Vector3d* ps, RSS& bv)
{
  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  const Vector3d& p3 = ps[2];
  Vector3d e[3];
  e[0] = p1 - p2;
  e[1] = p2 - p3;
  e[2] = p3 - p1;
  FCL_REAL len[3];
  len[0] = e[0].squaredNorm();
  len[1] = e[1].squaredNorm();
  len[2] = e[2].squaredNorm();

  int imax = 0;
  if(len[1] > len[0]) imax = 1;
  if(len[2] > len[imax]) imax = 2;

  bv.axis.col(2) = e[0].cross(e[1]).normalized();
  bv.axis.col(0) = e[imax].normalized();
  bv.axis.col(1) = bv.axis.col(2).cross(bv.axis.col(0));

  getRadiusAndOriginAndRectangleSize(ps, NULL, NULL, NULL, 3, bv.axis, bv.Tr, bv.l, bv.r);
}

void fit6(Vector3d* ps, RSS& bv)
{
  RSS bv1, bv2;
  fit3(ps, bv1);
  fit3(ps + 3, bv2);
  bv = bv1 + bv2;
}

void fitn(Vector3d* ps, int n, RSS& bv)
{
  Matrix3d M; // row first matrix
  Matrix3d E; // row first eigen-vectors
  Vector3d s = Vector3d::Zero();

  getCovariance(ps, NULL, NULL, NULL, n, M);
  eigen(M, s, E);
  axisFromEigen(E, s, bv.axis);

  // set rss origin, rectangle size and radius
  getRadiusAndOriginAndRectangleSize(ps, NULL, NULL, NULL, n, bv.axis, bv.Tr, bv.l, bv.r);
}

}

namespace kIOS_fit_functions
{

void fit1(Vector3d* ps, kIOS& bv)
{
  bv.num_spheres = 1;
  bv.spheres[0].o = ps[0];
  bv.spheres[0].r = 0;

  bv.obb.axis.setIdentity();
  bv.obb.extent.setZero();
  bv.obb.To = ps[0];
}

void fit2(Vector3d* ps, kIOS& bv)
{
  bv.num_spheres = 5;

  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  Vector3d p1p2 = p1 - p2;
  FCL_REAL len_p1p2 = p1p2.norm();
  p1p2.normalize();
 
  bv.obb.axis.col(0) = p1p2;
  generateCoordinateSystem(bv.obb.axis);
    
  FCL_REAL r0 = len_p1p2 * 0.5;
  bv.obb.extent << r0, 0, 0;
  bv.obb.To = (p1 + p2) * 0.5;

  bv.spheres[0].o = bv.obb.To;
  bv.spheres[0].r = r0;

  FCL_REAL r1 = r0 * invSinA;
  FCL_REAL r1cosA = r1 * cosA;
  bv.spheres[1].r = r1;
  bv.spheres[2].r = r1;
  Vector3d delta = bv.obb.axis.col(1) * r1cosA;
  bv.spheres[1].o = bv.spheres[0].o - delta;
  bv.spheres[2].o = bv.spheres[0].o + delta;

  bv.spheres[3].r = r1;
  bv.spheres[4].r = r1;
  delta = bv.obb.axis.col(2) * r1cosA;
  bv.spheres[3].o = bv.spheres[0].o - delta;
  bv.spheres[4].o = bv.spheres[0].o + delta;
}

void fit3(Vector3d* ps, kIOS& bv)
{
  bv.num_spheres = 3;
    
  const Vector3d& p1 = ps[0];
  const Vector3d& p2 = ps[1];
  const Vector3d& p3 = ps[2];
  Vector3d e[3];
  e[0] = p1 - p2;
  e[1] = p2 - p3;
  e[2] = p3 - p1;
  FCL_REAL len[3];
  len[0] = e[0].squaredNorm();
  len[1] = e[1].squaredNorm();
  len[2] = e[2].squaredNorm();
    
  int imax = 0;
  if(len[1] > len[0]) imax = 1;
  if(len[2] > len[imax]) imax = 2;
    
  bv.obb.axis.col(2) = e[0].cross(e[1]).normalized();
  bv.obb.axis.col(0) = e[imax].normalized();
  bv.obb.axis.col(1) = bv.obb.axis.col(2).cross(bv.obb.axis.col(0));

  getExtentAndCenter(ps, NULL, NULL, NULL, 3, bv.obb.axis, bv.obb.To, bv.obb.extent);

  // compute radius and center
  FCL_REAL r0;
  Vector3d center;
  circumCircleComputation(p1, p2, p3, center, r0);

  bv.spheres[0].o = center;
  bv.spheres[0].r = r0;

  FCL_REAL r1 = r0 * invSinA;
  Vector3d delta = bv.obb.axis.col(2) * (r1 * cosA);
  
  bv.spheres[1].r = r1;
  bv.spheres[1].o = center - delta;
  bv.spheres[2].r = r1;
  bv.spheres[2].o = center + delta;
}

void fitn(Vector3d* ps, int n, kIOS& bv)
{
  Matrix3d M;
  Matrix3d E;
  Vector3d s = Vector3d::Zero(); // three eigen values;

  getCovariance(ps, NULL, NULL, NULL, n, M);
  eigen(M, s, E);
  
  axisFromEigen(E, s, bv.obb.axis);

  getExtentAndCenter(ps, NULL, NULL, NULL, n, bv.obb.axis, bv.obb.To, bv.obb.extent);

  // get center and extension
  const Vector3d& center = bv.obb.To;
  const Vector3d& extent = bv.obb.extent;
  FCL_REAL r0 = maximumDistance(ps, NULL, NULL, NULL, n, center);
  
  // decide the k in kIOS
  if(extent[0] > kIOS_RATIO * extent[2])
  {
    if(extent[0] > kIOS_RATIO * extent[1]) bv.num_spheres = 5;
    else bv.num_spheres = 3;
  }
  else bv.num_spheres = 1;

  
  bv.spheres[0].o = center;
  bv.spheres[0].r = r0;

  if(bv.num_spheres >= 3)
  {
    FCL_REAL r10 = sqrt(r0 * r0 - extent[2] * extent[2]) * invSinA;
    Vector3d delta = bv.obb.axis.col(2) * (r10 * cosA - extent[2]);
    bv.spheres[1].o = center - delta;
    bv.spheres[2].o = center + delta;
   
    FCL_REAL r11 = 0, r12 = 0;
    r11 = maximumDistance(ps, NULL, NULL, NULL, n, bv.spheres[1].o);
    r12 = maximumDistance(ps, NULL, NULL, NULL, n, bv.spheres[2].o);
    bv.spheres[1].o += bv.obb.axis.col(2) * (-r10 + r11);
    bv.spheres[2].o += bv.obb.axis.col(2) * (r10 - r12);

    bv.spheres[1].r = r10;
    bv.spheres[2].r = r10;
  }

  if(bv.num_spheres >= 5)
  {
    FCL_REAL r10 = bv.spheres[1].r;
    Vector3d delta = bv.obb.axis.col(1) * (sqrt(r10 * r10 - extent[0] * extent[0] - extent[2] * extent[2]) - extent[1]);
    bv.spheres[3].o = bv.spheres[0].o - delta;
    bv.spheres[4].o = bv.spheres[0].o + delta;
    
    FCL_REAL r21 = 0, r22 = 0;
    r21 = maximumDistance(ps, NULL, NULL, NULL, n, bv.spheres[3].o);
    r22 = maximumDistance(ps, NULL, NULL, NULL, n, bv.spheres[4].o);

    bv.spheres[3].o += bv.obb.axis.col(1) * (-r10 + r21);
    bv.spheres[4].o += bv.obb.axis.col(1) * (r10 - r22);
    
    bv.spheres[3].r = r10;
    bv.spheres[4].r = r10;
  }
}

}

namespace OBBRSS_fit_functions
{
void fit1(Vector3d* ps, OBBRSS& bv)
{
  OBB_fit_functions::fit1(ps, bv.obb);
  RSS_fit_functions::fit1(ps, bv.rss);
}

void fit2(Vector3d* ps, OBBRSS& bv)
{
  OBB_fit_functions::fit2(ps, bv.obb);
  RSS_fit_functions::fit2(ps, bv.rss);
}

void fit3(Vector3d* ps, OBBRSS& bv)
{
  OBB_fit_functions::fit3(ps, bv.obb);
  RSS_fit_functions::fit3(ps, bv.rss);
}

void fitn(Vector3d* ps, int n, OBBRSS& bv)
{
  OBB_fit_functions::fitn(ps, n, bv.obb);
  RSS_fit_functions::fitn(ps, n, bv.rss);
}

}



template<>
void fit(Vector3d* ps, int n, OBB& bv)
{
  switch(n)
  {
  case 1:
    OBB_fit_functions::fit1(ps, bv);
    break;
  case 2:
    OBB_fit_functions::fit2(ps, bv);
    break;
  case 3:
    OBB_fit_functions::fit3(ps, bv);
    break;
  case 6:
    OBB_fit_functions::fit6(ps, bv);
    break;
  default:
    OBB_fit_functions::fitn(ps, n, bv);
  }
}


template<>
void fit(Vector3d* ps, int n, RSS& bv)
{
  switch(n)
  {
  case 1:
    RSS_fit_functions::fit1(ps, bv);
    break;
  case 2:
    RSS_fit_functions::fit2(ps, bv);
    break;
  case 3:
    RSS_fit_functions::fit3(ps, bv);
    break;
  default:
    RSS_fit_functions::fitn(ps, n, bv);
  }
}

template<>
void fit(Vector3d* ps, int n, kIOS& bv)
{
  switch(n)
  {
  case 1:
    kIOS_fit_functions::fit1(ps, bv);
    break;
  case 2:
    kIOS_fit_functions::fit2(ps, bv);
    break;
  case 3:
    kIOS_fit_functions::fit3(ps, bv);
    break;
  default:
    kIOS_fit_functions::fitn(ps, n, bv);
  }
}

template<>
void fit(Vector3d* ps, int n, OBBRSS& bv)
{
  switch(n)
  {
  case 1:
    OBBRSS_fit_functions::fit1(ps, bv);
    break;
  case 2:
    OBBRSS_fit_functions::fit2(ps, bv);
    break;
  case 3:
    OBBRSS_fit_functions::fit3(ps, bv);
    break;
  default:
    OBBRSS_fit_functions::fitn(ps, n, bv);
  }
}


OBB BVFitter<OBB>::fit(unsigned int* primitive_indices, int num_primitives)
{
  OBB bv;

  Matrix3d M; // row first matrix
  Matrix3d E; // row first eigen-vectors
  Vector3d s; // three eigen values

  getCovariance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, M);
  eigen(M, s, E);

  axisFromEigen(E, s, bv.axis);

  // set obb centers and extensions
  getExtentAndCenter(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.axis, bv.To, bv.extent);

  return bv;
}

OBBRSS BVFitter<OBBRSS>::fit(unsigned int* primitive_indices, int num_primitives)
{
  OBBRSS bv;
  Matrix3d M;
  Matrix3d E;
  Vector3d s;

  getCovariance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, M);
  eigen(M, s, E);

  axisFromEigen(E, s, bv.obb.axis);
  bv.rss.axis = bv.obb.axis;

  getExtentAndCenter(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.obb.axis, bv.obb.To, bv.obb.extent);

  Vector3d origin;
  FCL_REAL l[2];
  FCL_REAL r;
  getRadiusAndOriginAndRectangleSize(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.rss.axis, origin, l, r);

  bv.rss.Tr = origin;
  bv.rss.l[0] = l[0];
  bv.rss.l[1] = l[1];
  bv.rss.r = r;

  return bv;
}

RSS BVFitter<RSS>::fit(unsigned int* primitive_indices, int num_primitives)
{
  RSS bv;

  Matrix3d M; // row first matrix
  Matrix3d E; // row first eigen-vectors
  Vector3d s; // three eigen values
  getCovariance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, M);
  eigen(M, s, E);
  axisFromEigen(E, s, bv.axis);

  // set rss origin, rectangle size and radius

  Vector3d origin;
  FCL_REAL l[2];
  FCL_REAL r;
  getRadiusAndOriginAndRectangleSize(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.axis, origin, l, r);

  bv.Tr = origin;
  bv.l[0] = l[0];
  bv.l[1] = l[1];
  bv.r = r;


  return bv;
}


kIOS BVFitter<kIOS>::fit(unsigned int* primitive_indices, int num_primitives)
{
  kIOS bv;

  Matrix3d M; // row first matrix
  Matrix3d E; // row first eigen-vectors
  Vector3d s;
  
  getCovariance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, M);
  eigen(M, s, E);

  axisFromEigen(E, s, bv.obb.axis);

  // get centers and extensions
  getExtentAndCenter(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.obb.axis, bv.obb.To, bv.obb.extent);

  const Vector3d& center = bv.obb.To;
  const Vector3d& extent = bv.obb.extent;
  FCL_REAL r0 = maximumDistance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, center);

  // decide k in kIOS
  if(extent[0] > kIOS_RATIO * extent[2])
  {
    if(extent[0] > kIOS_RATIO * extent[1]) bv.num_spheres = 5;
    else bv.num_spheres = 3;
  }
  else bv.num_spheres = 1;

  bv.spheres[0].o = center;
  bv.spheres[0].r = r0;

  if(bv.num_spheres >= 3)
  {
    FCL_REAL r10 = sqrt(r0 * r0 - extent[2] * extent[2]) * invSinA;
    Vector3d delta = bv.obb.axis.col(2) * (r10 * cosA - extent[2]);
    bv.spheres[1].o = center - delta;
    bv.spheres[2].o = center + delta;

    FCL_REAL r11 = maximumDistance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.spheres[1].o);
    FCL_REAL r12 = maximumDistance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.spheres[2].o);

    bv.spheres[1].o += bv.obb.axis.col(2) * (-r10 + r11);
    bv.spheres[2].o += bv.obb.axis.col(2) * (r10 - r12);

    bv.spheres[1].r = r10;
    bv.spheres[2].r = r10;
  }

  if(bv.num_spheres >= 5)
  {
    FCL_REAL r10 = bv.spheres[1].r;
    Vector3d delta = bv.obb.axis.col(1) * (sqrt(r10 * r10 - extent[0] * extent[0] - extent[2] * extent[2]) - extent[1]);
    bv.spheres[3].o = bv.spheres[0].o - delta;
    bv.spheres[4].o = bv.spheres[0].o + delta;
    
    FCL_REAL r21 = 0, r22 = 0;
    r21 = maximumDistance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.spheres[3].o);
    r22 = maximumDistance(vertices, prev_vertices, tri_indices, primitive_indices, num_primitives, bv.spheres[4].o);

    bv.spheres[3].o += bv.obb.axis.col(1) * (-r10 + r21);
    bv.spheres[4].o += bv.obb.axis.col(1) * (r10 - r22);
    
    bv.spheres[3].r = r10;
    bv.spheres[4].r = r10;
  }

  return bv;
}


}