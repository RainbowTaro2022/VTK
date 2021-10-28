vtk_module_find_package(PACKAGE OpenXR)

set(classes
  vtkOpenXRRenderWindow
  vtkOpenXRRenderWindowInteractor
  vtkOpenXRManager
  vtkOpenXRCamera
  vtkOpenXRControlsHelper
  vtkOpenXRModel
  vtkOpenXRRenderer
  vtkOpenXRInteractorStyle)

vtk_module_add_module(VTK::RenderingOpenXR
  CLASSES ${classes}
  NOWRAP_HEADERS "vtkOpenXR.h" "XrExtensions.h" "vtkOpenXRUtilities.h"
  )

vtk_module_link(VTK::RenderingOpenXR
  PUBLIC
    OpenXR::OpenXR)

set(openxr_input_files
  vtk_openxr_actions.json
  vtk_openxr_binding_htc_vive_controller.json
  vtk_openxr_binding_khr_simple_controller.json
  vtk_openxr_binding_valve_index_controller.json
)

foreach(inputfile ${openxr_input_files})
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/${inputfile}
    ${CMAKE_BINARY_DIR}/${input_file}
    COPYONLY)
endforeach()

install(
  FILES ${openxr_input_files}
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  COMPONENT   runtime
)