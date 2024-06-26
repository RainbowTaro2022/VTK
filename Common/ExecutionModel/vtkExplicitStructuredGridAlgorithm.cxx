// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkExplicitStructuredGridAlgorithm.h"

#include "vtkCommand.h"
#include "vtkExplicitStructuredGrid.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkObjectFactory.h"
#include "vtkStreamingDemandDrivenPipeline.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkExplicitStructuredGridAlgorithm);

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//------------------------------------------------------------------------------
vtkExplicitStructuredGridAlgorithm::vtkExplicitStructuredGridAlgorithm()
{
  // by default assume filters have one input and one output
  // subclasses that deviate should modify this setting
  this->SetNumberOfInputPorts(1);
  this->SetNumberOfOutputPorts(1);
}

//------------------------------------------------------------------------------
vtkExplicitStructuredGrid* vtkExplicitStructuredGridAlgorithm::GetOutput()
{
  return this->GetOutput(0);
}

//------------------------------------------------------------------------------
vtkExplicitStructuredGrid* vtkExplicitStructuredGridAlgorithm::GetOutput(int port)
{
  return vtkExplicitStructuredGrid::SafeDownCast(this->GetOutputDataObject(port));
}

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::SetOutput(vtkDataObject* d)
{
  this->GetExecutive()->SetOutputData(0, d);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkExplicitStructuredGridAlgorithm::GetInput()
{
  return this->GetInput(0);
}

//------------------------------------------------------------------------------
vtkDataObject* vtkExplicitStructuredGridAlgorithm::GetInput(int port)
{
  return this->GetExecutive()->GetInputData(port, 0);
}

//------------------------------------------------------------------------------
vtkExplicitStructuredGrid* vtkExplicitStructuredGridAlgorithm::GetExplicitStructuredGridInput(
  int port)
{
  return vtkExplicitStructuredGrid::SafeDownCast(this->GetInput(port));
}

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::SetInputData(vtkDataObject* input)
{
  this->SetInputData(0, input);
}

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::SetInputData(int index, vtkDataObject* input)
{
  this->SetInputDataInternal(index, input);
}

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::AddInputData(vtkDataObject* input)
{
  this->AddInputData(0, input);
}

//------------------------------------------------------------------------------
void vtkExplicitStructuredGridAlgorithm::AddInputData(int index, vtkDataObject* input)
{
  this->AddInputDataInternal(index, input);
}

//------------------------------------------------------------------------------
vtkTypeBool vtkExplicitStructuredGridAlgorithm::ProcessRequest(
  vtkInformation* request, vtkInformationVector** inputVector, vtkInformationVector* outputVector)
{
  // generate the data
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
  {
    return this->RequestData(request, inputVector, outputVector);
  }

  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
  {
    return this->RequestUpdateExtent(request, inputVector, outputVector);
  }

  if (request->Has(vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_TIME()))
  {
    return this->RequestUpdateTime(request, inputVector, outputVector);
  }

  // execute information
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_INFORMATION()))
  {
    return this->RequestInformation(request, inputVector, outputVector);
  }
  if (request->Has(vtkDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
  {
    vtkInformation* outputInfo = outputVector->GetInformationObject(0);
    vtkExplicitStructuredGrid* grid = vtkExplicitStructuredGrid::New();
    outputInfo->Set(vtkDataObject::DATA_OBJECT(), grid);
    grid->Delete();
    return 1;
  }
  return this->Superclass::ProcessRequest(request, inputVector, outputVector);
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::RequestInformation(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector), vtkInformationVector* vtkNotUsed(outputVector))
{
  // do nothing let subclasses handle it
  return 1;
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::RequestUpdateTime(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector), vtkInformationVector* vtkNotUsed(outputVector))
{
  return 1;
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::RequestUpdateExtent(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** inputVector, vtkInformationVector* vtkNotUsed(outputVector))
{
  int numInputPorts = this->GetNumberOfInputPorts();
  for (int i = 0; i < numInputPorts; i++)
  {
    int numInputConnections = this->GetNumberOfInputConnections(i);
    for (int j = 0; j < numInputConnections; j++)
    {
      vtkInformation* inputInfo = inputVector[i]->GetInformationObject(j);
      inputInfo->Set(vtkStreamingDemandDrivenPipeline::EXACT_EXTENT(), 0);
    }
  }
  return 1;
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::RequestData(vtkInformation* vtkNotUsed(request),
  vtkInformationVector** vtkNotUsed(inputVector), vtkInformationVector* vtkNotUsed(outputVector))
{
  return 0;
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::FillOutputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  // now add our info
  info->Set(vtkDataObject::DATA_TYPE_NAME(), "vtkExplicitStructuredGrid");
  return 1;
}

//------------------------------------------------------------------------------
int vtkExplicitStructuredGridAlgorithm::FillInputPortInformation(
  int vtkNotUsed(port), vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkExplicitStructuredGrid");
  return 1;
}
VTK_ABI_NAMESPACE_END
