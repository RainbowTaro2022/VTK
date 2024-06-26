find_package(ADIOS2 2.4 REQUIRED
  COMPONENTS CXX
  OPTIONAL_COMPONENTS MPI)

if (TARGET VTK::ParallelMPI)
  set(TestADIOS2BPReaderSingleTimeStep_NUMPROCS 2)
  # For now vtkMultiBlockVolumeMapper does not support rendering in parallel
  set(TestADIOS2BPReaderMultiTimeSteps_NUMPROCS 2)
  set(TestADIOS2BPReaderMultiTimeSteps2D_NUMPROCS 2)
  vtk_add_test_mpi(vtkIOADIOS2CxxTests-MPI mpiTests TESTING_DATA
    TestADIOS2BPReaderMPISingleTimeStep.cxx
    TestADIOS2BPReaderMPIMultiTimeSteps3D.cxx,NO_VALID
    TestADIOS2BPReaderMPIMultiTimeSteps2D.cxx)
  vtk_test_cxx_executable(vtkIOADIOS2CxxTests-MPI mpiTests)

  # VTX tests
  set(TestIOADIOS2VTX_VTI3DRendering_NUMPROCS 2)
  vtk_add_test_mpi(vtkIOADIOS2VTXCxxTests-MPI mpiTests TESTING_DATA NO_OUTPUT
    TestIOADIOS2VTX_VTI3DRendering.cxx,NO_VALID)

  vtk_test_cxx_executable(vtkIOADIOS2VTXCxxTests-MPI mpiTests)
  target_link_libraries(vtkIOADIOS2VTXCxxTests-MPI PUBLIC adios2::adios2)
else ()
  vtk_add_test_cxx(vtkIOADIOS2CxxTests tests TESTING_DATA
    TestADIOS2BPReaderSingleTimeStep.cxx
    TestADIOS2BPReaderMultiTimeSteps3D.cxx
    TestADIOS2BPReaderMultiTimeSteps2D.cxx)

  vtk_test_cxx_executable(vtkIOADIOS2CxxTests tests)
endif ()

# VTX tests
vtk_add_test_cxx(vtkIOADIOS2VTXCxxTests tests TESTING_DATA NO_OUTPUT
  UnitTestIOADIOS2VTX.cxx,NO_VALID
  #TestIOADIOS2VTX_VTI3D.cxx,
  TestIOADIOS2VTX_VTI3DRendering.cxx,NO_VALID
  #TestIOADIOS2VTX_VTU3D.cxx,NO_VALID
  TestIOADIOS2VTX_VTU3DRendering.cxx,NO_VALID
  TestIOADIOS2VTX_VTU2DRendering.cxx,NO_VALID
  TestIOADIOS2VTX_VTU1DRendering.cxx,NO_VALID)

vtk_test_cxx_executable(vtkIOADIOS2VTXCxxTests tests)
target_link_libraries(vtkIOADIOS2VTXCxxTests PUBLIC adios2::adios2)
