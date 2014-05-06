vtk_module(vtkFiltersParallel
  GROUPS
    StandAlone
  DEPENDS
    vtkParallelCore
    vtkFiltersExtraction
    vtkRenderingCore
    vtkFiltersModeling
    vtkFiltersGeometry
  TEST_DEPENDS
    vtkParallelMPI
    vtkTestingCore
    vtkTestingRendering
    vtkInteractionStyle
    vtkRenderingOpenGL
    vtkRenderingParallel
    vtkFiltersParallelMPI
    vtkFiltersParallelImaging
    vtkIOLegacy
  )
