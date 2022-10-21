/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkAffineArray.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#ifndef vtkAffineArray_h
#define vtkAffineArray_h

#ifdef VTK_AFFINE_ARRAY_INSTANTIATING
#define VTK_GDA_VALUERANGE_INSTANTIATING
#include "vtkDataArrayPrivate.txx"
#include "vtkGenericDataArray.h"
#undef VTK_GDA_VALUERANGE_INSTANTIATING
#endif

#include "vtkAffineImplicitBackend.h" // for the array backend
#include "vtkImplicitArray.h"

/**
 * \var vtkAffineArray
 * \brief A utility alias for wrapping affine functions in implicit arrays
 *
 * In order to be usefully included in the dispatchers, these arrays need to be instantiated at the
 * vtk library compile time.
 *
 * @sa
 * vtkImplicitArray vtkAffineImplicitBackend
 */

VTK_ABI_NAMESPACE_BEGIN
template <typename T>
using vtkAffineArray = vtkImplicitArray<vtkAffineImplicitBackend<T>>;
VTK_ABI_NAMESPACE_END

#endif // vtkAffineArray_h

#ifdef VTK_AFFINE_ARRAY_INSTANTIATING

#define VTK_INSTANTIATE_AFFINE_ARRAY(ValueType)                                                    \
  VTK_ABI_NAMESPACE_BEGIN                                                                          \
  template class VTKCOMMONCORE_EXPORT vtkImplicitArray<vtkAffineImplicitBackend<ValueType>>;       \
  VTK_ABI_NAMESPACE_END                                                                            \
  namespace vtkDataArrayPrivate                                                                    \
  {                                                                                                \
  VTK_ABI_NAMESPACE_BEGIN                                                                          \
  VTK_INSTANTIATE_VALUERANGE_ARRAYTYPE(                                                            \
    vtkImplicitArray<vtkAffineImplicitBackend<ValueType>>, double)                                 \
  VTK_ABI_NAMESPACE_END                                                                            \
  }

#elif defined(VTK_USE_EXTERN_TEMPLATE)
#ifndef VTK_AFFINE_ARRAY_EXTERN
#define VTK_AFFINE_ARRAY_EXTERN
#ifdef _MSC_VER
#pragma warning(push)
// The following is needed when the vtkImplicitArray is declared
// dllexport and is used from another class in vtkCommonCore
#pragma warning(disable : 4910) // extern and dllexport incompatible
#endif
VTK_ABI_NAMESPACE_BEGIN
vtkInstantiateSecondOrderTemplateMacro(
  extern template class VTKCOMMONCORE_EXPORT vtkImplicitArray, vtkAffineImplicitBackend);
VTK_ABI_NAMESPACE_END
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // VTK_AFFINE_ARRAY_TEMPLATE_EXTERN

#endif // VTK_AFFINE_ARRAY_INSTANTIATING
