// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkLineWidget2.h"
#include "vtkCallbackCommand.h"
#include "vtkCommand.h"
#include "vtkEvent.h"
#include "vtkHandleWidget.h"
#include "vtkLineRepresentation.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkPointHandleRepresentation3D.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkWidgetCallbackMapper.h"
#include "vtkWidgetEvent.h"
#include "vtkWidgetEventTranslator.h"

#include <algorithm>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkLineWidget2);

//------------------------------------------------------------------------------
vtkLineWidget2::vtkLineWidget2()
{
  this->WidgetState = vtkLineWidget2::Start;
  this->ManagesCursor = 1;
  this->CurrentHandle = 0;

  // The widgets for moving the end points. They observe this widget (i.e.,
  // this widget is the parent to the handles).
  this->Point1Widget = vtkHandleWidget::New();
  this->Point1Widget->SetPriority(this->Priority - 0.01);
  this->Point1Widget->SetParent(this);
  this->Point1Widget->ManagesCursorOff();

  this->Point2Widget = vtkHandleWidget::New();
  this->Point2Widget->SetPriority(this->Priority - 0.01);
  this->Point2Widget->SetParent(this);
  this->Point2Widget->ManagesCursorOff();

  this->LineHandle = vtkHandleWidget::New();
  this->LineHandle->SetPriority(this->Priority - 0.01);
  this->LineHandle->SetParent(this);
  this->LineHandle->ManagesCursorOff();

  // Define widget events
  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, vtkLineWidget2::SelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent,
    vtkWidgetEvent::EndSelect, this, vtkLineWidget2::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonPressEvent,
    vtkWidgetEvent::Translate, this, vtkLineWidget2::TranslateAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonReleaseEvent,
    vtkWidgetEvent::EndTranslate, this, vtkLineWidget2::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::RightButtonPressEvent, vtkWidgetEvent::Scale, this, vtkLineWidget2::ScaleAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::RightButtonReleaseEvent,
    vtkWidgetEvent::EndScale, this, vtkLineWidget2::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, vtkLineWidget2::MoveAction);

  this->KeyEventCallbackCommand = vtkCallbackCommand::New();
  this->KeyEventCallbackCommand->SetClientData(this);
  this->KeyEventCallbackCommand->SetCallback(vtkLineWidget2::ProcessKeyEvents);
}

//------------------------------------------------------------------------------
vtkLineWidget2::~vtkLineWidget2()
{
  this->Point1Widget->Delete();
  this->Point2Widget->Delete();
  this->LineHandle->Delete();
  this->KeyEventCallbackCommand->Delete();
}

//------------------------------------------------------------------------------
void vtkLineWidget2::SetEnabled(int enabling)
{
  int enabled = this->Enabled;

  // We do this step first because it sets the CurrentRenderer
  this->Superclass::SetEnabled(enabling);

  // We defer enabling the handles until the selection process begins
  if (enabling && !enabled)
  {
    // Don't actually turn these on until cursor is near the end points or the line.
    this->CreateDefaultRepresentation();
    this->Point1Widget->SetRepresentation(
      reinterpret_cast<vtkLineRepresentation*>(this->WidgetRep)->GetPoint1Representation());
    this->Point1Widget->SetInteractor(this->Interactor);
    this->Point1Widget->GetRepresentation()->SetRenderer(this->CurrentRenderer);

    this->Point2Widget->SetRepresentation(
      reinterpret_cast<vtkLineRepresentation*>(this->WidgetRep)->GetPoint2Representation());
    this->Point2Widget->SetInteractor(this->Interactor);
    this->Point2Widget->GetRepresentation()->SetRenderer(this->CurrentRenderer);

    this->LineHandle->SetRepresentation(
      reinterpret_cast<vtkLineRepresentation*>(this->WidgetRep)->GetLineHandleRepresentation());
    this->LineHandle->SetInteractor(this->Interactor);
    this->LineHandle->GetRepresentation()->SetRenderer(this->CurrentRenderer);

    if (this->Parent)
    {
      this->Parent->AddObserver(
        vtkCommand::KeyPressEvent, this->KeyEventCallbackCommand, this->Priority);
      this->Parent->AddObserver(
        vtkCommand::KeyReleaseEvent, this->KeyEventCallbackCommand, this->Priority);
    }
    else
    {
      this->Interactor->AddObserver(
        vtkCommand::KeyPressEvent, this->KeyEventCallbackCommand, this->Priority);
      this->Interactor->AddObserver(
        vtkCommand::KeyReleaseEvent, this->KeyEventCallbackCommand, this->Priority);
    }
  }
  else if (!enabling && enabled)
  {
    this->Point1Widget->SetEnabled(0);
    this->Point2Widget->SetEnabled(0);
    this->LineHandle->SetEnabled(0);

    if (this->Parent)
    {
      this->Parent->RemoveObserver(this->KeyEventCallbackCommand);
    }
    else
    {
      this->Interactor->RemoveObserver(this->KeyEventCallbackCommand);
    }
  }
}

//------------------------------------------------------------------------------
void vtkLineWidget2::SelectAction(vtkAbstractWidget* w)
{
  vtkLineWidget2* self = reinterpret_cast<vtkLineWidget2*>(w);
  if (self->WidgetRep->GetInteractionState() == vtkLineRepresentation::Outside)
  {
    return;
  }

  // Get the event position
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // We are definitely selected
  self->WidgetState = vtkLineWidget2::Active;
  self->GrabFocus(self->EventCallbackCommand);
  double e[2];
  e[0] = static_cast<double>(X);
  e[1] = static_cast<double>(Y);
  reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)->StartWidgetInteraction(e);
  self->InvokeEvent(vtkCommand::LeftButtonPressEvent, nullptr); // for the handles
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
  self->EventCallbackCommand->SetAbortFlag(1);
}

//------------------------------------------------------------------------------
void vtkLineWidget2::TranslateAction(vtkAbstractWidget* w)
{
  vtkLineWidget2* self = reinterpret_cast<vtkLineWidget2*>(w);
  if (self->WidgetRep->GetInteractionState() == vtkLineRepresentation::Outside)
  {
    return;
  }

  // Modify the state, we are selected
  int state = self->WidgetRep->GetInteractionState();
  if (state == vtkLineRepresentation::OnP1)
  {
    reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)
      ->SetInteractionState(vtkLineRepresentation::TranslatingP1);
  }
  else if (state == vtkLineRepresentation::OnP2)
  {
    reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)
      ->SetInteractionState(vtkLineRepresentation::TranslatingP2);
  }
  else
  {
    reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)
      ->SetInteractionState(vtkLineRepresentation::OnLine);
  }

  // Get the event position
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // We are definitely selected
  self->WidgetState = vtkLineWidget2::Active;
  self->GrabFocus(self->EventCallbackCommand);
  double eventPos[2];
  eventPos[0] = static_cast<double>(X);
  eventPos[1] = static_cast<double>(Y);
  reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)->StartWidgetInteraction(eventPos);
  self->InvokeEvent(vtkCommand::LeftButtonPressEvent, nullptr); // for the handles
  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

//------------------------------------------------------------------------------
void vtkLineWidget2::ScaleAction(vtkAbstractWidget* w)
{
  vtkLineWidget2* self = reinterpret_cast<vtkLineWidget2*>(w);
  if (self->WidgetRep->GetInteractionState() == vtkLineRepresentation::Outside)
  {
    return;
  }

  reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)
    ->SetInteractionState(vtkLineRepresentation::Scaling);
  self->Interactor->Disable();
  self->LineHandle->SetEnabled(0);
  self->Interactor->Enable();

  // Get the event position
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // We are definitely selected
  self->WidgetState = vtkLineWidget2::Active;
  self->GrabFocus(self->EventCallbackCommand);
  double eventPos[2];
  eventPos[0] = static_cast<double>(X);
  eventPos[1] = static_cast<double>(Y);
  reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)->StartWidgetInteraction(eventPos);
  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

//------------------------------------------------------------------------------
void vtkLineWidget2::MoveAction(vtkAbstractWidget* w)
{
  vtkLineWidget2* self = reinterpret_cast<vtkLineWidget2*>(w);
  // compute some info we need for all cases
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // See whether we're active
  if (self->WidgetState == vtkLineWidget2::Start)
  {
    self->Interactor->Disable(); // avoid extra renders
    self->Point1Widget->SetEnabled(0);
    self->Point2Widget->SetEnabled(0);
    self->LineHandle->SetEnabled(0);

    int oldState = self->WidgetRep->GetInteractionState();
    int state = self->WidgetRep->ComputeInteractionState(X, Y);
    int changed;
    // Determine if we are near the end points or the line
    if (state == vtkLineRepresentation::Outside)
    {
      changed = self->RequestCursorShape(VTK_CURSOR_DEFAULT);
    }
    else // must be near something
    {
      changed = self->RequestCursorShape(VTK_CURSOR_HAND);
      if (state == vtkLineRepresentation::OnP1)
      {
        self->Point1Widget->SetEnabled(1);
      }
      else if (state == vtkLineRepresentation::OnP2)
      {
        self->Point2Widget->SetEnabled(1);
      }
      else // if ( state == vtkLineRepresentation::OnLine )
      {
        self->LineHandle->SetEnabled(1);
        changed = 1; // movement along the line always needs render
      }
    }
    self->Interactor->Enable(); // avoid extra renders
    if (changed || oldState != state)
    {
      self->Render();
    }
  }
  else // if ( self->WidgetState == vtkLineWidget2::Active )
  {
    // moving something
    double e[2];
    e[0] = static_cast<double>(X);
    e[1] = static_cast<double>(Y);
    self->InvokeEvent(vtkCommand::MouseMoveEvent, nullptr); // handles observe this
    reinterpret_cast<vtkLineRepresentation*>(self->WidgetRep)->WidgetInteraction(e);
    self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
    self->EventCallbackCommand->SetAbortFlag(1);
    self->Render();
  }
}

//------------------------------------------------------------------------------
void vtkLineWidget2::EndSelectAction(vtkAbstractWidget* w)
{
  vtkLineWidget2* self = reinterpret_cast<vtkLineWidget2*>(w);
  if (self->WidgetState == vtkLineWidget2::Start)
  {
    return;
  }

  // Return state to not active
  self->WidgetState = vtkLineWidget2::Start;
  self->ReleaseFocus();
  self->InvokeEvent(vtkCommand::LeftButtonReleaseEvent, nullptr); // handles observe this
  self->EventCallbackCommand->SetAbortFlag(1);
  self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
  self->Superclass::EndInteraction();
  self->Render();
}

//------------------------------------------------------------------------------
void vtkLineWidget2::CreateDefaultRepresentation()
{
  if (!this->WidgetRep)
  {
    this->WidgetRep = vtkLineRepresentation::New();
  }
}

//------------------------------------------------------------------------------
void vtkLineWidget2::SetProcessEvents(vtkTypeBool pe)
{
  this->Superclass::SetProcessEvents(pe);

  this->Point1Widget->SetProcessEvents(pe);
  this->Point2Widget->SetProcessEvents(pe);
  this->LineHandle->SetProcessEvents(pe);
}

//------------------------------------------------------------------------------
void vtkLineWidget2::ProcessKeyEvents(vtkObject*, unsigned long event, void* clientdata, void*)
{
  vtkLineWidget2* self = static_cast<vtkLineWidget2*>(clientdata);
  vtkLineRepresentation* rep = vtkLineRepresentation::SafeDownCast(self->WidgetRep);
  char* cKeySym = self->Interactor->GetKeySym();
  std::string keySym = cKeySym != nullptr ? cKeySym : "";
  std::transform(keySym.begin(), keySym.end(), keySym.begin(), ::toupper);
  if (event == vtkCommand::KeyPressEvent)
  {
    if (keySym == "X")
    {
      rep->GetPoint1Representation()->SetXTranslationAxisOn();
      rep->GetPoint2Representation()->SetXTranslationAxisOn();
      rep->GetLineHandleRepresentation()->SetXTranslationAxisOn();
      rep->GetPoint1Representation()->SetConstrained(true);
      rep->GetPoint2Representation()->SetConstrained(true);
      rep->GetLineHandleRepresentation()->SetConstrained(true);
    }
    else if (keySym == "Y")
    {
      rep->GetPoint1Representation()->SetYTranslationAxisOn();
      rep->GetPoint2Representation()->SetYTranslationAxisOn();
      rep->GetLineHandleRepresentation()->SetYTranslationAxisOn();
      rep->GetPoint1Representation()->SetConstrained(true);
      rep->GetPoint2Representation()->SetConstrained(true);
      rep->GetLineHandleRepresentation()->SetConstrained(true);
    }
    else if (keySym == "Z")
    {
      rep->GetPoint1Representation()->SetZTranslationAxisOn();
      rep->GetPoint2Representation()->SetZTranslationAxisOn();
      rep->GetLineHandleRepresentation()->SetZTranslationAxisOn();
      rep->GetPoint1Representation()->SetConstrained(true);
      rep->GetPoint2Representation()->SetConstrained(true);
      rep->GetLineHandleRepresentation()->SetConstrained(true);
    }
    else if (keySym == "L")
    {
      double p1[3], p2[3], v[3];
      rep->GetPoint1WorldPosition(p1);
      rep->GetPoint2WorldPosition(p2);
      vtkMath::Subtract(p2, p1, v);
      vtkMath::Normalize(v);
      rep->GetPoint1Representation()->SetCustomTranslationAxisOn();
      rep->GetPoint1Representation()->SetCustomTranslationAxis(v);
      rep->GetPoint2Representation()->SetCustomTranslationAxisOn();
      rep->GetPoint2Representation()->SetCustomTranslationAxis(v);
      rep->GetLineHandleRepresentation()->SetCustomTranslationAxisOn();
      rep->GetLineHandleRepresentation()->SetCustomTranslationAxis(v);
      rep->GetPoint1Representation()->SetConstrained(true);
      rep->GetPoint2Representation()->SetConstrained(true);
      rep->GetLineHandleRepresentation()->SetConstrained(true);
    }
  }
  else if (event == vtkCommand::KeyReleaseEvent)
  {
    if (keySym == "L" || keySym == "X" || keySym == "Y" || keySym == "Z")
    {
      rep->GetPoint1Representation()->SetTranslationAxisOff();
      rep->GetPoint2Representation()->SetTranslationAxisOff();
      rep->GetLineHandleRepresentation()->SetTranslationAxisOff();
      rep->GetPoint1Representation()->SetConstrained(false);
      rep->GetPoint2Representation()->SetConstrained(false);
      rep->GetLineHandleRepresentation()->SetConstrained(false);
    }
  }
}

//------------------------------------------------------------------------------
void vtkLineWidget2::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
VTK_ABI_NAMESPACE_END
