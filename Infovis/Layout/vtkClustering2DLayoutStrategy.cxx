// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-FileCopyrightText: Copyright 2008 Sandia Corporation
// SPDX-License-Identifier: LicenseRef-BSD-3-Clause-Sandia-USGov

#include "vtkClustering2DLayoutStrategy.h"

#include "vtkBitArray.h"
#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCommand.h"
#include "vtkDataArray.h"
#include "vtkDoubleArray.h"
#include "vtkEdgeListIterator.h"
#include "vtkFastSplatter.h"
#include "vtkFloatArray.h"
#include "vtkGraph.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkLogger.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPoints.h"
#include "vtkSmartPointer.h"
#include "vtkTree.h"

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkClustering2DLayoutStrategy);

// Cool-down function.
static inline float CoolDown(float t, float r)
{
  return t - (t / r);
}

//------------------------------------------------------------------------------

vtkClustering2DLayoutStrategy::vtkClustering2DLayoutStrategy()
{

  // Create internal vtk classes
  this->DensityGrid = vtkSmartPointer<vtkFastSplatter>::New();
  this->SplatImage = vtkSmartPointer<vtkImageData>::New();
  this->RepulsionArray = vtkSmartPointer<vtkFloatArray>::New();
  this->AttractionArray = vtkSmartPointer<vtkFloatArray>::New();
  this->EdgeCountArray = vtkSmartPointer<vtkIntArray>::New();

  this->RandomSeed = 123;
  this->MaxNumberOfIterations = 200;
  this->IterationsPerLayout = 200;
  this->InitialTemperature = 5;
  this->CoolDownRate = 50.0;
  this->LayoutComplete = 0;
  this->EdgeWeightField = nullptr;
  this->SetEdgeWeightField("weight");
  this->RestDistance = 0;
  this->EdgeArray = nullptr;
  this->CuttingThreshold = 0;
}

//------------------------------------------------------------------------------

vtkClustering2DLayoutStrategy::~vtkClustering2DLayoutStrategy()
{
  this->SetEdgeWeightField(nullptr);
}

// Helper functions
void vtkClustering2DLayoutStrategy::GenerateCircularSplat(vtkImageData* splat, int x, int y)
{
  splat->SetDimensions(x, y, 1);
  splat->AllocateScalars(VTK_FLOAT, 1);

  const int* dimensions = splat->GetDimensions();

  // Circular splat: 1 in the middle and 0 at the corners and sides
  for (int row = 0; row < dimensions[1]; ++row)
  {
    for (int col = 0; col < dimensions[0]; ++col)
    {
      float splatValue;

      // coordinates will range from -1 to 1
      float xCoord = (col - dimensions[0] / 2.0) / (dimensions[0] / 2.0);
      float yCoord = (row - dimensions[1] / 2.0) / (dimensions[1] / 2.0);

      float radius = sqrt(xCoord * xCoord + yCoord * yCoord);
      if ((1 - radius) > 0)
      {
        splatValue = 1 - radius;
      }
      else
      {
        splatValue = 0;
      }

      // Set value
      splat->SetScalarComponentFromFloat(col, row, 0, 0, splatValue);
    }
  }
}

void vtkClustering2DLayoutStrategy::GenerateGaussianSplat(vtkImageData* splat, int x, int y)
{
  splat->SetDimensions(x, y, 1);
  splat->AllocateScalars(VTK_FLOAT, 1);

  const int* dimensions = splat->GetDimensions();

  // Gaussian splat
  float falloff = 10; // fast falloff
  float e = 2.71828182845904;

  for (int row = 0; row < dimensions[1]; ++row)
  {
    for (int col = 0; col < dimensions[0]; ++col)
    {
      float splatValue;

      // coordinates will range from -1 to 1
      float xCoord = (col - dimensions[0] / 2.0) / (dimensions[0] / 2.0);
      float yCoord = (row - dimensions[1] / 2.0) / (dimensions[1] / 2.0);

      splatValue = pow(e, -((xCoord * xCoord + yCoord * yCoord) * falloff));

      // Set value
      splat->SetScalarComponentFromFloat(col, row, 0, 0, splatValue);
    }
  }
}

//------------------------------------------------------------------------------
// Set the graph that will be laid out
void vtkClustering2DLayoutStrategy::Initialize()
{
  vtkMath::RandomSeed(this->RandomSeed);

  // Set up some quick access variables
  vtkPoints* pts = this->Graph->GetPoints();
  vtkIdType numVertices = this->Graph->GetNumberOfVertices();
  vtkIdType numEdges = this->Graph->GetNumberOfEdges();

  // Make sure output point type is float
  if (pts->GetData()->GetDataType() != VTK_FLOAT)
  {
    vtkErrorMacro("Layout strategy expects to have points of type float");
    this->LayoutComplete = 1;
    return;
  }

  // Get a quick pointer to the point data
  vtkFloatArray* array = vtkArrayDownCast<vtkFloatArray>(pts->GetData());
  float* rawPointData = array->GetPointer(0);

  // Avoid divide by zero
  float div = 1;
  if (numVertices > 0)
  {
    div = static_cast<float>(numVertices);
  }

  // The optimal distance between vertices.
  if (this->RestDistance == 0)
  {
    this->RestDistance = sqrt(1.0 / div);
  }

  // Set up array to store repulsion values
  this->RepulsionArray->SetNumberOfComponents(3);
  this->RepulsionArray->SetNumberOfTuples(numVertices);
  for (vtkIdType i = 0; i < numVertices * 3; ++i)
  {
    this->RepulsionArray->SetValue(i, 0);
  }

  // Set up array to store attraction values
  this->AttractionArray->SetNumberOfComponents(3);
  this->AttractionArray->SetNumberOfTuples(numVertices);
  for (vtkIdType i = 0; i < numVertices * 3; ++i)
  {
    this->AttractionArray->SetValue(i, 0);
  }

  // Put the edge data into compact, fast access edge data structure
  delete[] this->EdgeArray;
  this->EdgeArray = new vtkLayoutEdge[numEdges];

  // Store the number of edges associated with each vertex
  this->EdgeCountArray->SetNumberOfComponents(1);
  this->EdgeCountArray->SetNumberOfTuples(numVertices);
  for (vtkIdType i = 0; i < numVertices; ++i)
  {
    this->EdgeCountArray->SetValue(i, this->Graph->GetDegree(i));
  }

  // Jitter x and y, skip z
  for (vtkIdType i = 0; i < numVertices * 3; i += 3)
  {
    rawPointData[i] += this->RestDistance * (vtkMath::Random() - .5);
    rawPointData[i + 1] += this->RestDistance * (vtkMath::Random() - .5);
  }

  // Get the weight array
  vtkDataArray* weightArray = nullptr;
  double weight, maxWeight = 1;
  if (this->WeightEdges && this->EdgeWeightField != nullptr)
  {
    weightArray = vtkArrayDownCast<vtkDataArray>(
      this->Graph->GetEdgeData()->GetAbstractArray(this->EdgeWeightField));
    if (weightArray != nullptr)
    {
      for (vtkIdType w = 0; w < weightArray->GetNumberOfTuples(); w++)
      {
        weight = weightArray->GetTuple1(w);
        if (weight > maxWeight)
        {
          maxWeight = weight;
        }
      }
    }
  }

  // Load up the edge data structures
  vtkSmartPointer<vtkEdgeListIterator> edges = vtkSmartPointer<vtkEdgeListIterator>::New();
  this->Graph->GetEdges(edges);
  while (edges->HasNext())
  {
    vtkEdgeType e = edges->Next();
    this->EdgeArray[e.Id].from = e.Source;
    this->EdgeArray[e.Id].to = e.Target;
    this->EdgeArray[e.Id].dead_edge = 0;

    if (weightArray != nullptr)
    {
      weight = weightArray->GetTuple1(e.Id);
      float normalized_weight = weight / maxWeight;

      // Now increase the effect of high weight edges
      // Note: This is full of magic goodness
      normalized_weight = pow(normalized_weight, 4);

      this->EdgeArray[e.Id].weight = normalized_weight;
    }
    else
    {
      this->EdgeArray[e.Id].weight = 1.0;
    }
  }

  // Set some vars
  this->TotalIterations = 0;
  this->LayoutComplete = 0;
  this->Temp = this->InitialTemperature;
  this->CuttingThreshold = 10000 * this->RestDistance; // Max cut length

  // Set up the image splatter
  this->GenerateGaussianSplat(this->SplatImage, 41, 41);
  this->DensityGrid->SetInputData(1, this->SplatImage);
  this->DensityGrid->SetOutputDimensions(100, 100, 1);
}

//------------------------------------------------------------------------------

// Simple graph layout method
void vtkClustering2DLayoutStrategy::Layout()
{
  // Do I have a graph to layout
  if (this->Graph == nullptr)
  {
    vtkErrorMacro("Graph Layout called with Graph==nullptr, call SetGraph(g) first");
    this->LayoutComplete = 1;
    return;
  }

  // Set my graph as input into the density grid
  this->DensityGrid->SetInputData(this->Graph);

  // Set up some variables
  vtkPoints* pts = this->Graph->GetPoints();
  vtkIdType numVertices = this->Graph->GetNumberOfVertices();
  vtkIdType numEdges = this->Graph->GetNumberOfEdges();

  // Get a quick pointer to the point data
  vtkFloatArray* array = vtkArrayDownCast<vtkFloatArray>(pts->GetData());
  float* rawPointData = array->GetPointer(0);

  // This is the mega, uber, triple inner loop
  // ye of weak hearts, tread no further!
  float delta[] = { 0, 0, 0 };
  float disSquared;
  float attractValue;
  float epsilon = 1e-5;
  vtkIdType rawSourceIndex = 0;
  vtkIdType rawTargetIndex = 0;
  for (int i = 0; i < this->IterationsPerLayout; ++i)
  {

    // Initialize the repulsion and attraction arrays
    for (vtkIdType j = 0; j < numVertices * 3; ++j)
    {
      this->RepulsionArray->SetValue(j, 0);
    }

    // Set up array to store attraction values
    for (vtkIdType j = 0; j < numVertices * 3; ++j)
    {
      this->AttractionArray->SetValue(j, 0);
    }

    // Compute bounds of graph going into the density grid
    double bounds[6], paddedBounds[6];
    this->Graph->ComputeBounds();
    this->Graph->GetBounds(bounds);

    // Give bounds a 10% padding
    paddedBounds[0] = bounds[0] - (bounds[1] - bounds[0]) * .1;
    paddedBounds[1] = bounds[1] + (bounds[1] - bounds[0]) * .1;
    paddedBounds[2] = bounds[2] - (bounds[3] - bounds[2]) * .1;
    paddedBounds[3] = bounds[3] + (bounds[3] - bounds[2]) * .1;
    paddedBounds[4] = paddedBounds[5] = 0;

    // Update the density grid
    this->DensityGrid->SetModelBounds(paddedBounds);
    this->DensityGrid->Update();

    // Sanity check scalar type
    if (this->DensityGrid->GetOutput()->GetScalarType() != VTK_FLOAT)
    {
      vtkErrorMacro("DensityGrid expected to be of type float");
      return;
    }

    // Get the array handle
    float* densityArray = static_cast<float*>(this->DensityGrid->GetOutput()->GetScalarPointer());

    // Get the dimensions of the density grid
    int dims[3];
    this->DensityGrid->GetOutputDimensions(dims);

    // Calculate the repulsive forces
    float* rawRepulseArray = this->RepulsionArray->GetPointer(0);
    for (vtkIdType j = 0; j < numVertices; ++j)
    {
      rawSourceIndex = j * 3;

      // Compute indices into the density grid
      int indexX = static_cast<int>((rawPointData[rawSourceIndex] - paddedBounds[0]) /
          (paddedBounds[1] - paddedBounds[0]) * dims[0] +
        .5);
      int indexY = static_cast<int>((rawPointData[rawSourceIndex + 1] - paddedBounds[2]) /
          (paddedBounds[3] - paddedBounds[2]) * dims[1] +
        .5);

      // Look up the gradient density within the density grid
      float x1 = densityArray[indexY * dims[0] + indexX - 1];
      float x2 = densityArray[indexY * dims[0] + indexX + 1];
      float y1 = densityArray[(indexY - 1) * dims[0] + indexX];
      float y2 = densityArray[(indexY + 1) * dims[0] + indexX];

      rawRepulseArray[rawSourceIndex] = (x1 - x2); // Push away from higher
      rawRepulseArray[rawSourceIndex + 1] = (y1 - y2);
    }

    // Calculate the attractive forces
    float* rawAttractArray = this->AttractionArray->GetPointer(0);
    for (vtkIdType j = 0; j < numEdges; ++j)
    {

      // Check for dead edge
      if (this->EdgeArray[j].dead_edge)
      {
        continue;
      }

      rawSourceIndex = this->EdgeArray[j].from * 3;
      rawTargetIndex = this->EdgeArray[j].to * 3;

      // No need to attract points to themselves
      if (rawSourceIndex == rawTargetIndex)
        continue;

      delta[0] = rawPointData[rawSourceIndex] - rawPointData[rawTargetIndex];
      delta[1] = rawPointData[rawSourceIndex + 1] - rawPointData[rawTargetIndex + 1];
      disSquared = delta[0] * delta[0] + delta[1] * delta[1];

      // Compute a bunch of parameters used below
      int sourceIndex = this->EdgeArray[j].from;
      int targetIndex = this->EdgeArray[j].to;
      int numSourceEdges = this->EdgeCountArray->GetValue(sourceIndex);
      int numTargetEdges = this->EdgeCountArray->GetValue(targetIndex);

      // Perform weight adjustment
      attractValue = this->EdgeArray[j].weight * disSquared - this->RestDistance;
      rawAttractArray[rawSourceIndex] -= delta[0] * attractValue;
      rawAttractArray[rawSourceIndex + 1] -= delta[1] * attractValue;
      rawAttractArray[rawTargetIndex] += delta[0] * attractValue;
      rawAttractArray[rawTargetIndex + 1] += delta[1] * attractValue;

      // This logic forces edge lengths to be short
      if (numSourceEdges < 10)
      {
        rawPointData[rawSourceIndex] -= delta[0] * .45;
        rawPointData[rawSourceIndex + 1] -= delta[1] * .45;
      }
      else if (numTargetEdges < 10)
      {
        rawPointData[rawTargetIndex] += delta[0] * .45;
        rawPointData[rawTargetIndex + 1] += delta[1] * .45;
      }

      // Cutting edges for clustering
      if (disSquared > this->CuttingThreshold)
      {
        if (((numSourceEdges > 1) && (numTargetEdges > 1)))
        {
          this->EdgeArray[j].dead_edge = 1;
          this->EdgeCountArray->SetValue(sourceIndex, numSourceEdges - 1);
          this->EdgeCountArray->SetValue(targetIndex, numTargetEdges - 1);
        }
      }
    }

    // Okay now set new positions based on replusion
    // and attraction 'forces'
    for (vtkIdType j = 0; j < numVertices; ++j)
    {
      rawSourceIndex = j * 3;

      // Get forces for this node
      float forceX = rawAttractArray[rawSourceIndex] + rawRepulseArray[rawSourceIndex];
      float forceY = rawAttractArray[rawSourceIndex + 1] + rawRepulseArray[rawSourceIndex + 1];

      // Forces can get extreme so limit them
      // Note: This is pseudo-normalization of the
      //       force vector, just to save some cycles

      // Avoid divide by zero
      float forceDiv = fabs(forceX) + fabs(forceY) + epsilon;
      float pNormalize = vtkMath::Min(1.0f, 1.0f / forceDiv);
      pNormalize *= this->Temp;
      forceX *= pNormalize;
      forceY *= pNormalize;

      rawPointData[rawSourceIndex] += forceX;
      rawPointData[rawSourceIndex + 1] += forceY;
    }

    // The point coordinates have been modified
    this->Graph->GetPoints()->Modified();

    // Reduce temperature as layout approaches a better configuration.
    this->Temp = CoolDown(this->Temp, this->CoolDownRate);

    // Announce progress
    double progress =
      (i + this->TotalIterations) / static_cast<double>(this->MaxNumberOfIterations);
    this->InvokeEvent(vtkCommand::ProgressEvent, static_cast<void*>(&progress));

    // Adjust cutting
    float maxCutLength = 10000 * this->RestDistance;
    float minCutLength = 100 * this->RestDistance;
    this->CuttingThreshold = maxCutLength * (1 - progress) * (1 - progress) + minCutLength;

  } // End loop this->IterationsPerLayout

  // Check for completion of layout
  this->TotalIterations += this->IterationsPerLayout;
  if (this->TotalIterations >= this->MaxNumberOfIterations)
  {

    // Make sure no vertex is on top of another vertex
    this->ResolveCoincidentVertices();

    // I'm done
    this->LayoutComplete = 1;
  }

  // Mark points as modified
  this->Graph->GetPoints()->Modified();
}

void vtkClustering2DLayoutStrategy::ResolveCoincidentVertices()
{

  // Note: This algorithm is stupid but was easy to implement
  //       please change or improve if you'd like. :)

  // Basically see if the vertices are within a tolerance
  // of each other (do they fall into the same bucket).
  // If the vertices do fall into the same bucket give them
  // some random displacements to resolve coincident and
  // repeat until we have no coincident vertices

  // Get the number of vertices in the graph datastructure
  vtkIdType numVertices = this->Graph->GetNumberOfVertices();

  // Get a quick pointer to the point data
  vtkPoints* pts = this->Graph->GetPoints();
  vtkFloatArray* array = vtkArrayDownCast<vtkFloatArray>(pts->GetData());
  float* rawPointData = array->GetPointer(0);

  // Place the vertices into a giant grid (100xNumVertices)
  // and see if you have any collisions
  vtkBitArray* giantGrid = vtkBitArray::New();
  vtkIdType xDim = static_cast<int>(sqrt(static_cast<double>(numVertices)) * 10);
  vtkIdType yDim = static_cast<int>(sqrt(static_cast<double>(numVertices)) * 10);
  vtkIdType gridSize = xDim * yDim;
  giantGrid->SetNumberOfValues(gridSize);

  // Initialize array to zeros
  for (vtkIdType i = 0; i < gridSize; ++i)
  {
    giantGrid->SetValue(i, 0);
  }

  double bounds[6], paddedBounds[6];
  this->Graph->GetBounds(bounds);

  // Give bounds a 10% padding
  paddedBounds[0] = bounds[0] - (bounds[1] - bounds[0]) * .1;
  paddedBounds[1] = bounds[1] + (bounds[1] - bounds[0]) * .1;
  paddedBounds[2] = bounds[2] - (bounds[3] - bounds[2]) * .1;
  paddedBounds[3] = bounds[3] + (bounds[3] - bounds[2]) * .1;
  paddedBounds[4] = paddedBounds[5] = 0;

  int totalCollisionOps = 0;

  for (vtkIdType i = 0; i < numVertices; ++i)
  {
    int rawIndex = i * 3;

    // Compute indices into the buckets
    int indexX = static_cast<int>((rawPointData[rawIndex] - paddedBounds[0]) /
        (paddedBounds[1] - paddedBounds[0]) * (xDim - 1) +
      .5);
    int indexY = static_cast<int>((rawPointData[rawIndex + 1] - paddedBounds[2]) /
        (paddedBounds[3] - paddedBounds[2]) * (yDim - 1) +
      .5);

    // See if you collide with another vertex
    if (giantGrid->GetValue(indexX + indexY * xDim))
    {

      // Oh my... try to get yourself out of this
      // by randomly jumping to a place that doesn't
      // have another vertex
      bool collision = true;
      float jumpDistance = 5.0 * (paddedBounds[1] - paddedBounds[0]) / xDim; // 2.5 grid spaces max
      int collisionOps = 0;

      // You get 10 tries and then we have to punt
      while (collision && (collisionOps < 10))
      {
        collisionOps++;

        // Move
        rawPointData[rawIndex] += jumpDistance * (vtkMath::Random() - .5);
        rawPointData[rawIndex + 1] += jumpDistance * (vtkMath::Random() - .5);

        // Test
        indexX = static_cast<int>((rawPointData[rawIndex] - paddedBounds[0]) /
            (paddedBounds[1] - paddedBounds[0]) * (xDim - 1) +
          .5);
        indexY = static_cast<int>((rawPointData[rawIndex + 1] - paddedBounds[2]) /
            (paddedBounds[3] - paddedBounds[2]) * (yDim - 1) +
          .5);
        if (!giantGrid->GetValue(indexX + indexY * xDim))
        {
          collision = false; // yea
        }
      } // while
      totalCollisionOps += collisionOps;
    } // if collide

    // Put into a bucket
    giantGrid->SetValue(indexX + indexY * xDim, 1);
  }

  // Delete giantGrid
  giantGrid->Initialize();
  giantGrid->Delete();

  // Report number of collision operations just for sanity check
  vtkLog(TRACE, "Collision Ops: " << totalCollisionOps);
}

void vtkClustering2DLayoutStrategy::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "RandomSeed: " << this->RandomSeed << endl;
  os << indent << "MaxNumberOfIterations: " << this->MaxNumberOfIterations << endl;
  os << indent << "IterationsPerLayout: " << this->IterationsPerLayout << endl;
  os << indent << "InitialTemperature: " << this->InitialTemperature << endl;
  os << indent << "CoolDownRate: " << this->CoolDownRate << endl;
  os << indent << "RestDistance: " << this->RestDistance << endl;
  os << indent << "CuttingThreshold: " << this->CuttingThreshold << endl;
  os << indent << "EdgeWeightField: " << (this->EdgeWeightField ? this->EdgeWeightField : "(none)")
     << endl;
}
VTK_ABI_NAMESPACE_END
